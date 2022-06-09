// Copyright 2022 Patrick Hirsh
// SPDX-License-Identifier: GPL-2.0-or-later
#include "uniform.h"
#include "ch.h"
#include "chtrace.h"
#include "chvt.h"
#include "debug.h"
#include "hal.h"
#include "hal_pal.h"
#include "math.h"
#include "print.h"


// ======================================================================
// Uniform EEPROM
// ======================================================================
//
// uint32 (4-byte) KB EEPROM layout:
// _______________________________________________________________________
// | <unused2> | <unused1> | <status led brightness> | <status led mode> |
// ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

static void uniform_eeprom_write_status_led_mode(uint8_t mode) {

    // unpack    
    uint32_t eeprom = eeconfig_read_kb();
    //uint8_t unpacked_mode =             (eeprom >> 0) & 0xFF;
    uint8_t unpacked_brightness =       (eeprom >> 8) & 0xFF;
    uint8_t unpacked_unused1 =          (eeprom >> 16) & 0xFF;
    uint8_t unpacked_unused2 =          (eeprom >> 24) & 0xFF;
    
    // pack
    uint32_t packed_mode =              mode;
    uint32_t packed_brightness =        unpacked_brightness << 8;
    uint32_t packed_unused1 =           unpacked_unused1 << 16;
    uint32_t packed_unused2 =           unpacked_unused2 << 24;
    uint32_t payload =                  packed_unused2 | packed_unused1 | packed_brightness | packed_mode;
    eeconfig_update_kb(payload);
}

static uint8_t uniform_eeprom_read_status_led_mode(void) {
    return (eeconfig_read_kb() >> 0) & 0xFF;
}

static void uniform_eeprom_write_status_led_brightness(uint8_t brightness) {

    // unpack    
    uint32_t eeprom = eeconfig_read_kb();
    uint8_t unpacked_mode =             (eeprom >> 0) & 0xFF;
    //uint8_t unpacked_brightness =       (eeprom >> 8) & 0xFF;
    uint8_t unpacked_unused1 =          (eeprom >> 16) & 0xFF;
    uint8_t unpacked_unused2 =          (eeprom >> 24) & 0xFF;
    
    // pack
    uint32_t packed_mode =              unpacked_mode;
    uint32_t packed_brightness =        brightness << 8;
    uint32_t packed_unused1 =           unpacked_unused1 << 16;
    uint32_t packed_unused2 =           unpacked_unused2 << 24;
    uint32_t payload =                  packed_unused2 | packed_unused1 | packed_brightness | packed_mode;
    eeconfig_update_kb(payload);
}

static uint8_t uniform_eeprom_read_status_led_brightness(void) {
    return (eeconfig_read_kb() >> 8) & 0xFF;
}


// ======================================================================
// Status LEDs (State Info)
// ======================================================================

typedef struct {
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
} uniform_status_led_color;

static uniform_status_led_color status_leds[RGBLED_NUM];

static uint8_t uniform_status_led_brightness;
static bool uniform_mod_state_caps;
static bool uniform_mod_state_fn1;
static bool uniform_mod_state_fn2;


// ======================================================================
// Effect Utilities
// ======================================================================

// linearly interpolate between val1 and val2 based on strength (distance into interpolation)
/* COMMENTED OUT UNTIL USED
static uint8_t uniform_linear_interp(
    uint8_t val1,               // value we're transitioning from
    uint8_t val2,               // value we're transitioning to
    uint8_t strength,           // how far into the shift between val1 and val2 are we? (out of max_strength)
    uint8_t max_strength)       // when strength is at this value, we are entirely weighted on val2
{
    return ((float)(max_strength - strength) / (float)max_strength) * (float)val1 +        // val1 contribution
           ((float)(strength) / (float)max_strength) * (float)val2;                        // val2 contribution
}
*/

// sinusoidally interpolate between val1 and val2 based on strength (distance into interpolation)
static uint8_t uniform_sinusoidal_interpolation(
    uint8_t val1,               // value we're transitioning from
    uint8_t val2,               // value we're transitioning to
    uint8_t strength,           // how far into the shift between val1 and val2 are we? (out of max_strength)
    uint8_t max_strength)       // when strength is at this value, we are entirely weighted on val2
{
    // convert strength / max_strength ratio to fit the half-period of sin
    float x_raw = ((float)strength / (float)max_strength) * M_PI;

    // shift left by a quarter-period to get a segment that shifts from -1.0 to 1.0
    float x_shifted = x_raw - (0.5f * M_PI);

    // convert resulting value from -1.0:1.0 to 0.0:1.0
    float sinusoidal_progress = (sin(x_shifted) + 1.0f) / 2.0f;

    return (1.0f - sinusoidal_progress) * (float)val1 +        // val1 contribution
           sinusoidal_progress * (float)val2;                  // val2 contribution
}


// ======================================================================
// Mode: nightrider
// ======================================================================
#ifdef UNIFORM_STATUS_LED_MODE_NIGHTRIDER

static const uint8_t    nightrider_primary_hue = 0;

// trace effect
static const float      nightrider_trace_falloff_scalar = 0.6f;     // value to scale the perceived trace light emission falloff by (larger value = stronger falloff)
static const float      nightrider_trace_degredation_rate = 0.07f;  // rate to degrade trace position every tick when the mod key isn't held
static const float      nightrider_trace_speed = 10;                // sin input (ticks) is scaled by this value to determine position (smaller value = faster)
static const float      nightrider_led0_pos = -1.0;                 // led position relative to the center of the LED cluster
static const float      nightrider_led1_pos = 0;                    // led position relative to the center of the LED cluster
static const float      nightrider_led2_pos = 1.0;                  // led position relative to the center of the LED cluster
static float            nightrider_trace_sinusoidal_x;              // nightrider_trace_sinusoidal_x = arcsin(nightrider_trace_xpos) * nightrider_trace_speed
static float            nightrider_trace_xpos;                      // nightrider_trace_xpos = sin(nightrider_trace_sinusoidal_x / nightrider_trace_speed)

static void uniform_init_status_leds_nightrider(void) {
    status_leds[0] = (uniform_status_led_color) { nightrider_primary_hue, 255, 0 };
    status_leds[1] = (uniform_status_led_color) { nightrider_primary_hue, 255, 0 };
    status_leds[2] = (uniform_status_led_color) { nightrider_primary_hue, 255, 0 };
}

static void uniform_update_status_leds_nightrider(void) { 

    // fn1 trace effect
    if (uniform_mod_state_fn1) {
        // advance along the dinusoidal path
        nightrider_trace_sinusoidal_x = nightrider_trace_sinusoidal_x + 1.0f;
        if ((float)nightrider_trace_sinusoidal_x / nightrider_trace_speed > 2 * M_PI) {
            nightrider_trace_sinusoidal_x = 0;
        }
        nightrider_trace_xpos = sin(nightrider_trace_sinusoidal_x / nightrider_trace_speed);
    }
    else {
        // if mod key isn't held, break from sinusodial path and linearly decrement actual nightrider_trace_xpos until we reach rest
        nightrider_trace_xpos = nightrider_trace_xpos + nightrider_trace_degredation_rate > 1.0f ? 1.0f : nightrider_trace_xpos + nightrider_trace_degredation_rate;

        // continuously rebase sinusoidal position as we do this
        nightrider_trace_sinusoidal_x = asin(nightrider_trace_xpos) * nightrider_trace_speed;
    }


    // led distance from trace (considering falloff)
    float led0_scaled_dist = nightrider_trace_falloff_scalar * fabs(nightrider_led0_pos - nightrider_trace_xpos);
    float led1_scaled_dist = nightrider_trace_falloff_scalar * fabs(nightrider_led1_pos - nightrider_trace_xpos);
    float led2_scaled_dist = nightrider_trace_falloff_scalar * fabs(nightrider_led2_pos - nightrider_trace_xpos);

    // determine effect intensity scalar (0.0f-1.0f)
    float led0_scale = 1.0f - led0_scaled_dist < 0 ? 0 : 1.0f - led0_scaled_dist;
    float led1_scale = 1.0f - led1_scaled_dist < 0 ? 0 : 1.0f - led1_scaled_dist;
    float led2_scale = 1.0f - led2_scaled_dist < 0 ? 0 : 1.0f - led2_scaled_dist;

    // apply resulting values to create trace effect
    status_leds[0].val = 255.0f * led0_scale;
    status_leds[1].val = 255.0f * led1_scale;
    status_leds[2].val = 255.0f * led2_scale;
}

static void uniform_nightrider_keypress_handle(uint16_t keycode, keyrecord_t *record) {

}
#endif // UNIFORM_STATUS_LED_MODE_NIGHTRIDER


// ======================================================================
// Mode: Imperial
// ======================================================================
#ifdef UNIFORM_STATUS_LED_MODE_IMPERIAL

static const uint8_t    imperial_primary_hue = 0;

// trace effect
static const float      imperial_trace_falloff_scalar = 0.6f;     // value to scale the perceived trace light emission falloff by (larger value = stronger falloff)
static const float      imperial_trace_speed = 10;                // sin input (ticks) is scaled by this value to determine position (smaller value = faster)
static const float      imperial_led0_pos = 0.0;                    
static const float      imperial_led1_pos = 1.0;                    
static const float      imperial_led2_pos = 2.0;                  
static int              imperial_trace_sinusoidal_pos;

static void uniform_init_status_leds_imperial(void) {
    status_leds[0] = (uniform_status_led_color) { imperial_primary_hue, 255, 0 };
    status_leds[1] = (uniform_status_led_color) { imperial_primary_hue, 255, 0 };
    status_leds[2] = (uniform_status_led_color) { imperial_primary_hue, 255, 0 };
}

static void uniform_update_status_leds_imperial(void) {

    // fn1 trace effect
    float trace_absolute_pos = 2.0f;
    if (imperial_trace_sinusoidal_pos > 0) {
        imperial_trace_sinusoidal_pos++;
        if ((float)imperial_trace_sinusoidal_pos / imperial_trace_speed > M_PI) {
            imperial_trace_sinusoidal_pos = 0;
        }
        trace_absolute_pos = 2.0f - sin(imperial_trace_sinusoidal_pos / imperial_trace_speed) * 2.0f;
    }

    // led distance from trace (considering falloff)
    float led0_scaled_dist = imperial_trace_falloff_scalar * fabs(imperial_led0_pos - trace_absolute_pos);
    float led1_scaled_dist = imperial_trace_falloff_scalar * fabs(imperial_led1_pos - trace_absolute_pos);
    float led2_scaled_dist = imperial_trace_falloff_scalar * fabs(imperial_led2_pos - trace_absolute_pos);

    // determine effect intensity scalar (0.0f-1.0f)
    float led0_scale = 1.0f - led0_scaled_dist < 0 ? 0 : 1.0f - led0_scaled_dist;
    float led1_scale = 1.0f - led1_scaled_dist < 0 ? 0 : 1.0f - led1_scaled_dist;
    float led2_scale = 1.0f - led2_scaled_dist < 0 ? 0 : 1.0f - led2_scaled_dist;

    // apply resulting values to create trace effect
    status_leds[0].val = 255.0f * led0_scale;
    status_leds[1].val = 255.0f * led1_scale;
    status_leds[2].val = 255.0f * led2_scale;
}

static void uniform_imperial_keypress_handle(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        if ((float)imperial_trace_sinusoidal_pos / imperial_trace_speed > 0.5f * M_PI) {
            imperial_trace_sinusoidal_pos = (0.5f * M_PI - ((imperial_trace_sinusoidal_pos / imperial_trace_speed) - 0.5f * M_PI)) * imperial_trace_speed;
        }
        else {
            imperial_trace_sinusoidal_pos++;
        }
    }
}
#endif // UNIFORM_STATUS_LED_MODE_IMPERIAL



// ======================================================================
// Mode: Sorbet
// ======================================================================
#ifdef UNIFORM_STATUS_LED_MODE_SORBET

// color palette
static const uint8_t    sorbet_led0_hue = 235;                  
static const uint8_t    sorbet_led1_hue = 15;                   
static const uint8_t    sorbet_led2_hue = 5;                    
static const int        sorbet_led0_hue_shifted = 170;          
static const int        sorbet_led1_hue_shifted = 115;          
static const int        sorbet_led2_hue_shifted = 85;

// trace effect
static const float      sorbet_trace_falloff_scalar = 1.5f;     // value to scale the perceived trace light emission falloff by (larger value = stronger falloff)
static const float      sorbet_trace_speed = 35;                // sin input (ticks) is scaled by this value to determine position (smaller value = faster)
static const uint8_t    sorbet_trace_fade_time = 50;            // time (in ticks) to fade in / out of the effect
static const uint8_t    sorbet_trace_rest_pos = 75;             // should be a value between 0 and 2 * M_PI * sorbet_trace_speed (period of oscillation). trace will start here
static const float      sorbet_led0_pos = -0.5;                 // led position relative to the center of the LED cluster
static const float      sorbet_led1_pos = 0;                    // led position relative to the center of the LED cluster
static const float      sorbet_led2_pos = 0.5;                  // led position relative to the center of the LED cluster
static uint16_t         sorbet_trace_pos;
static uint8_t          sorbet_trace_str;

// hue shift effect           
static const uint8_t    sorbet_hue_shift_transition_time = 70;  // time (in ticks) to fade in / out of the effect
static uint8_t          sorbet_hue_shift_str;

static void uniform_init_status_leds_sorbet(void) {         
    sorbet_trace_pos = sorbet_trace_rest_pos;
    status_leds[0] = (uniform_status_led_color) { sorbet_led0_hue, 255, 255 };
    status_leds[1] = (uniform_status_led_color) { sorbet_led1_hue, 255, 255 };
    status_leds[2] = (uniform_status_led_color) { sorbet_led2_hue, 255, 255 };
}

static void uniform_update_status_leds_sorbet(void) { 

    // split space fn1 hue shift effect
    if (uniform_mod_state_fn1 || sorbet_hue_shift_str != 0) {
        
        // update hue shift strength based on mod key state
        if (sorbet_hue_shift_str < sorbet_hue_shift_transition_time && uniform_mod_state_fn1) {
            sorbet_hue_shift_str++;
        } 
        if (!uniform_mod_state_fn1 && sorbet_hue_shift_str != 0) {
            sorbet_hue_shift_str--;
        }

        // apply hue shift
        status_leds[0].hue = uniform_sinusoidal_interpolation(sorbet_led0_hue, sorbet_led0_hue_shifted, sorbet_hue_shift_str, sorbet_hue_shift_transition_time);
        status_leds[1].hue = uniform_sinusoidal_interpolation(sorbet_led1_hue, sorbet_led1_hue_shifted, sorbet_hue_shift_str, sorbet_hue_shift_transition_time);
        status_leds[2].hue = uniform_sinusoidal_interpolation(sorbet_led2_hue, sorbet_led2_hue_shifted, sorbet_hue_shift_str, sorbet_hue_shift_transition_time);
    }

    // caps lock trace effect
    if (uniform_mod_state_caps || sorbet_trace_str != 0) {

        // get trace position
        sorbet_trace_pos++;
        if ((float)sorbet_trace_pos / sorbet_trace_speed > 2 * M_PI) {
            sorbet_trace_pos = 0;
        }
        float trace_pos = sin(sorbet_trace_pos / sorbet_trace_speed);
        
        // update trace strength based on mod key state
        if (sorbet_trace_str < sorbet_trace_fade_time && uniform_mod_state_caps) {
            sorbet_trace_str++;
        } 
        if (!uniform_mod_state_caps && sorbet_trace_str != 0) {
            sorbet_trace_str--;
            // if trace strength completely dies out, reset position
            if (sorbet_trace_str == 0) {
                sorbet_trace_pos = sorbet_trace_rest_pos;
            }
        }

        // led distance from trace (considering falloff)
        float led0_scaled_dist = sorbet_trace_falloff_scalar * fabs(sorbet_led0_pos - trace_pos);
        float led1_scaled_dist = sorbet_trace_falloff_scalar * fabs(sorbet_led1_pos - trace_pos);
        float led2_scaled_dist = sorbet_trace_falloff_scalar * fabs(sorbet_led2_pos - trace_pos);

        // determine effect intensity scalar (0.0f-1.0f)
        float led0_scale = 1.0f - led0_scaled_dist < 0 ? 0 : 1.0f - led0_scaled_dist;
        float led1_scale = 1.0f - led1_scaled_dist < 0 ? 0 : 1.0f - led1_scaled_dist;
        float led2_scale = 1.0f - led2_scaled_dist < 0 ? 0 : 1.0f - led2_scaled_dist;

        // apply resulting saturation values to create trace effect
        status_leds[0].sat = 255 - (sorbet_trace_str / (float)sorbet_trace_fade_time) * 255.0f * led0_scale;
        status_leds[1].sat = 255 - (sorbet_trace_str / (float)sorbet_trace_fade_time) * 255.0f * led1_scale;
        status_leds[2].sat = 255 - (sorbet_trace_str / (float)sorbet_trace_fade_time) * 255.0f * led2_scale;
    }
}               

static void uniform_sorbet_keypress_handle(uint16_t keycode, keyrecord_t *record) {
    
}
#endif // UNIFORM_STATUS_LED_MODE_SORBET


// ======================================================================
// Mode: Rainbow
// ======================================================================
#ifdef UNIFORM_STATUS_LED_MODE_RAINBOW

static void uniform_init_status_leds_rainbow(void) {
    status_leds[0] = (uniform_status_led_color) { 0, 255, 255 };
    status_leds[1] = (uniform_status_led_color) { 20, 255, 255 };
    status_leds[2] = (uniform_status_led_color) { 40, 255, 255 };
}

static void uniform_update_status_leds_rainbow(void) { 
    status_leds[0] = (uniform_status_led_color) { status_leds[0].hue + 1, 255, 255 };
    status_leds[1] = (uniform_status_led_color) { status_leds[1].hue + 1, 255, 255 };
    status_leds[2] = (uniform_status_led_color) { status_leds[2].hue + 1, 255, 255 };
}

static void uniform_rainbow_keypress_handle(uint16_t keycode, keyrecord_t *record) {

}
#endif // UNIFORM_STATUS_LED_MODE_RAINBOW


// ======================================================================
// Status LEDs (Modes)
// ======================================================================

typedef struct {
    void(*init)(void);                                          // always invoked once when a mode is set active
    void(*update)(void);                                        // ticked once every UNIFORM_STATUS_LED_TICKRATE ticks
    void(*key_event)(uint16_t keycode, keyrecord_t *record);    // invoked anytime a key event occurs
} uniform_status_led_mode;

// master array of status led modes
static uniform_status_led_mode uniform_status_led_modes[] = {
#ifdef UNIFORM_STATUS_LED_MODE_NIGHTRIDER
    (uniform_status_led_mode) { uniform_init_status_leds_nightrider, uniform_update_status_leds_nightrider, uniform_nightrider_keypress_handle },
#endif
#ifdef UNIFORM_STATUS_LED_MODE_IMPERIAL
    (uniform_status_led_mode) { uniform_init_status_leds_imperial, uniform_update_status_leds_imperial, uniform_imperial_keypress_handle },
#endif
#ifdef UNIFORM_STATUS_LED_MODE_SORBET
    (uniform_status_led_mode) { uniform_init_status_leds_sorbet, uniform_update_status_leds_sorbet, uniform_sorbet_keypress_handle },
#endif
#ifdef UNIFORM_STATUS_LED_MODE_RAINBOW
    (uniform_status_led_mode) { uniform_init_status_leds_rainbow, uniform_update_status_leds_rainbow, uniform_rainbow_keypress_handle },
#endif
};
static uint8_t uniform_status_leds_mode_count;
static uint8_t uniform_status_leds_mode_index;


// ======================================================================
// Status LEDs
// ======================================================================

// apply final mode-independent effects to status led hue
static uint8_t uniform_status_led_post_process_hue(uint8_t hue) {
    return hue;
}

// apply final mode-independent effects to status led sat
static uint8_t uniform_status_led_post_process_sat(uint8_t sat) {
    return sat;
}

// apply final mode-independent effects to status led val
static uint8_t uniform_status_led_post_process_val(uint8_t val) {

    // apply settings layer pulse effect
    const uint16_t rest_pos = 60;
    static uint16_t pulse_pos = rest_pos;
    static uint8_t pulse_str = 0;
    if (uniform_mod_state_fn2 || pulse_str != 0) {
        
        const uint8_t fade_time = 50;       // time (in ticks) to fade in / out of the pulse effect
        const float pulse_speed = 50.0f;    // lower value = faster pulse time

        // update pulse position
        pulse_pos++;
        if ((float)pulse_pos / pulse_speed > 2 * M_PI) {
            pulse_pos = 0;
        }
        float settings_pulse_sin_pos = sin((float)pulse_pos / pulse_speed);

        // update pulse strength
        if (pulse_str < fade_time && uniform_mod_state_fn2) {
            pulse_str++;
        } 
        if (!uniform_mod_state_fn2 && pulse_str != 0) {
            pulse_str--;
            if (pulse_str == 0) {
                // bring pulse back to rest if it completely dies out
                pulse_pos = rest_pos;
            }
        }

        // linearly interpolate to smooth out pulse effect transition
        val = ((float)(fade_time - pulse_str) / (float)fade_time) * (float)(val) +                                                          // base val contribution
              ((float)(pulse_str) / (float)fade_time) * (((float)val * ((settings_pulse_sin_pos + 1.0f) / 4.0f)) + (float)val * 0.5f);      // pulse-applied val contribution
    }

    // apply final brightness post-processing
    return (float)val * ((float)uniform_status_led_brightness / 100.0f);
}

// deferred executor callback
uint32_t uniform_tick_status_leds(uint32_t trigger_time, void* cb_arg) {
    uniform_update_status_leds();
    return UNIFORM_STATUS_LED_TICKRATE;
}

//status led update function
void uniform_update_status_leds(void) {
    uniform_status_led_modes[uniform_status_leds_mode_index].update();
    for (int i = 0; i < RGBLED_NUM; i++) {
        rgblight_sethsv_at(
            uniform_status_led_post_process_hue(status_leds[i].hue), 
            uniform_status_led_post_process_sat(status_leds[i].sat), 
            uniform_status_led_post_process_val(status_leds[i].val), 
            i);
    }
}

// notify current mode of key event
void uniform_key_event(uint16_t keycode, keyrecord_t *record) {
    uniform_status_led_modes[uniform_status_leds_mode_index].key_event(keycode, record);
}

void uniform_increment_status_leds_mode(void) {
    uniform_status_leds_mode_index++;
    if (uniform_status_leds_mode_index == uniform_status_leds_mode_count) {
        uniform_status_leds_mode_index = 0;
    }
    // any time we change modes, always invoke the mode init function and update eeprom
    uniform_status_led_modes[uniform_status_leds_mode_index].init();
    uniform_eeprom_write_status_led_mode(uniform_status_leds_mode_index);
}

void uniform_decrement_status_leds_mode(void) {
    if (uniform_status_leds_mode_index == 0) {
        uniform_status_leds_mode_index = uniform_status_leds_mode_count - 1;
    } 
    else {
        uniform_status_leds_mode_index--;
    }
    // any time we change modes, always invoke the mode init function and update eeprom
    uniform_status_led_modes[uniform_status_leds_mode_index].init();
    uniform_eeprom_write_status_led_mode(uniform_status_leds_mode_index);
}

void uniform_increase_status_leds_brightness(void) {
    uniform_status_led_brightness = uniform_status_led_brightness + 5;
    if (uniform_status_led_brightness > 100) { 
        uniform_status_led_brightness = 100;
    }
    // update eeprom
    uniform_eeprom_write_status_led_brightness(uniform_status_led_brightness);
}

void uniform_decrease_status_leds_brightness(void) {
    if (uniform_status_led_brightness < 5) {
        uniform_status_led_brightness = 0;
    }
    else {
        uniform_status_led_brightness = uniform_status_led_brightness - 5;
    }
    // update eeprom
    uniform_eeprom_write_status_led_brightness(uniform_status_led_brightness);
}

void uniform_flip_mod_state_caps(void) {
    uniform_mod_state_caps = !uniform_mod_state_caps;
} 

void uniform_set_mod_state_fn1(bool state) {
    uniform_mod_state_fn1 = state;
} 

void uniform_set_mod_state_fn2(bool state) {
    uniform_mod_state_fn2 = state;
} 


// ======================================================================
// Uniform
// ======================================================================

void matrix_init_kb(void) {
    matrix_init_user();
    
    // status led brightness
    uniform_status_led_brightness = uniform_eeprom_read_status_led_brightness() % 5 != 0 || uniform_eeprom_read_status_led_brightness() > 100 ? 
        100 : 
        uniform_eeprom_read_status_led_brightness();
    
    // status led observed layer / modifier states
    uniform_mod_state_caps = false;
    uniform_mod_state_fn1 = false;
    uniform_mod_state_fn2 = false;
    
    // status led modes
    uniform_status_leds_mode_count = sizeof(uniform_status_led_modes) / sizeof(uniform_status_led_mode);
    uniform_status_leds_mode_index = uniform_eeprom_read_status_led_mode() > uniform_status_leds_mode_count - 1 ? 0 : uniform_eeprom_read_status_led_mode();
    uniform_status_led_modes[uniform_status_leds_mode_index].init();

    // defered executor for updating status LEDs
    setPinOutput(RGB_DI_PIN);
    defer_exec(1, uniform_tick_status_leds, NULL);
}

void housekeeping_task_kb(void) {

}
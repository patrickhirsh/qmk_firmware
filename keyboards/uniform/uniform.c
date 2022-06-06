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
// Status LEDs (Modes)
// ======================================================================

typedef struct {
    void(*init)(void);
    void(*update)(void);
} uniform_status_led_mode;

// master array of status led modes
static uniform_status_led_mode uniform_status_led_modes[] = {
    (uniform_status_led_mode) { uniform_init_status_leds_sorbet, uniform_update_status_leds_sorbet },
    (uniform_status_led_mode) { uniform_init_status_leds_rainbow, uniform_update_status_leds_rainbow }
};
static uint8_t uniform_status_leds_mode_count;
static uint8_t uniform_status_leds_mode_index;


// ======================================================================
// Mode: Sorbet
// ======================================================================

static const float      sorbet_trace_falloff_scalar = 1.5f;     // distance the light spreads from the trace point
static const float      sorbet_trace_speed = 35;                // sin input (ticks) is scaled by this value to determine position (smaller value = faster)
static const uint8_t    sorbet_trace_fade_time = 50;            // time (in ticks) to fade in / out of the effect
static const uint8_t    sorbet_trace_rest_pos = 75;             // should be a value between 0 and 2 * M_PI * sorbet_trace_speed (period of oscillation). trace will start here
static const float      sorbet_led0_pos = -0.5;                 // led position relative to the center of the LED cluster
static const float      sorbet_led1_pos = 0;                    // led position relative to the center of the LED cluster
static const float      sorbet_led2_pos = 0.5;                  // led position relative to the center of the LED cluster

static uint16_t         sorbet_trace_pos;
static uint8_t          sorbet_trace_str;

void uniform_init_status_leds_sorbet(void) {
    sorbet_trace_pos = sorbet_trace_rest_pos;
    status_leds[0] = (uniform_status_led_color) { 220, 255, 255 };
    status_leds[1] = (uniform_status_led_color) { 15, 255, 255 };
    status_leds[2] = (uniform_status_led_color) { 5, 255, 255 };
}

void uniform_update_status_leds_sorbet(void) { 

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
            // mod key down.. increase strength
            sorbet_trace_str++;
        } 
        if (!uniform_mod_state_caps && sorbet_trace_str != 0) {
            // mod key up.. reduce strength
            sorbet_trace_str--;

            // if trace strength completely dies out, reset position
            if (sorbet_trace_str == 0) {
                sorbet_trace_pos = sorbet_trace_rest_pos;
            }
        }

        float led0_scaled_dist = sorbet_trace_falloff_scalar * fabs(sorbet_led0_pos - trace_pos);
        float led1_scaled_dist = sorbet_trace_falloff_scalar * fabs(sorbet_led1_pos - trace_pos);
        float led2_scaled_dist = sorbet_trace_falloff_scalar * fabs(sorbet_led2_pos - trace_pos);

        float led0_scale = 1.0f - led0_scaled_dist < 0 ? 0 : 1.0f - led0_scaled_dist;
        float led1_scale = 1.0f - led1_scaled_dist < 0 ? 0 : 1.0f - led1_scaled_dist;
        float led2_scale = 1.0f - led2_scaled_dist < 0 ? 0 : 1.0f - led2_scaled_dist;

        status_leds[0] = (uniform_status_led_color) { status_leds[0].hue, 255 - (sorbet_trace_str / (float)sorbet_trace_fade_time) * 255.0f * led0_scale, status_leds[0].val };
        status_leds[1] = (uniform_status_led_color) { status_leds[1].hue, 255 - (sorbet_trace_str / (float)sorbet_trace_fade_time) * 255.0f * led1_scale, status_leds[1].val };
        status_leds[2] = (uniform_status_led_color) { status_leds[2].hue, 255 - (sorbet_trace_str / (float)sorbet_trace_fade_time) * 255.0f * led2_scale, status_leds[2].val };
    } 

    else {
        status_leds[0] = (uniform_status_led_color) { 220, 255, 255 };
        status_leds[1] = (uniform_status_led_color) { 15, 255, 255 };
        status_leds[2] = (uniform_status_led_color) { 5, 255, 255 };
    }
}


// ======================================================================
// Mode: Rainbow
// ======================================================================

void uniform_init_status_leds_rainbow(void) {
    status_leds[0] = (uniform_status_led_color) { 0, 255, 255 };
    status_leds[1] = (uniform_status_led_color) { 20, 255, 255 };
    status_leds[2] = (uniform_status_led_color) { 40, 255, 255 };
}

void uniform_update_status_leds_rainbow(void) { 
    status_leds[0] = (uniform_status_led_color) { status_leds[0].hue + 1, 255, 255 };
    status_leds[1] = (uniform_status_led_color) { status_leds[1].hue + 1, 255, 255 };
    status_leds[2] = (uniform_status_led_color) { status_leds[2].hue + 1, 255, 255 };
}


// ======================================================================
// Status LEDs
// ======================================================================

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

// apply final mode-independent effects to status led hue
uint8_t uniform_status_led_post_process_hue(uint8_t hue) {
    return hue;
}

// apply final mode-independent effects to status led sat
uint8_t uniform_status_led_post_process_sat(uint8_t sat) {
    return sat;
}

// apply final mode-independent effects to status led val
uint8_t uniform_status_led_post_process_val(uint8_t val) {

    // apply settings layer pulse effect
    static uint16_t pulse_pos = 40;
    static uint8_t pulse_str = 0;
    if (uniform_mod_state_fn2 || pulse_str != 0) {
        
        const uint8_t fade_time = 50;       // time (in ticks) to fade in / out of the pulse effect
        const float pulse_speed = 40.0f;    // lower value = faster pulse time

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
                pulse_pos = 60;
            }
        }

        // linearly interpolate to smooth out pulse effect transition
        val = ((float)(fade_time - pulse_str) / (float)fade_time) * (float)(val) +                                  // base val contribution
              ((float)(pulse_str) / (float)fade_time) * ((float)val * ((settings_pulse_sin_pos + 1.0f) / 2.0f));    // pulse-applied val contribution
    }

    // apply final brightness post-processing
    return (float)val * ((float)uniform_status_led_brightness / 100.0f);
}

void uniform_increment_status_leds_mode(void) {
    uniform_status_leds_mode_index++;
    if (uniform_status_leds_mode_index == uniform_status_leds_mode_count) {
        uniform_status_leds_mode_index = 0;
    }
    // any time we change modes, always invoke the mode init function
    uniform_status_led_modes[uniform_status_leds_mode_index].init();

    // update eeprom
    uniform_eeprom_write_status_led_mode(uniform_status_leds_mode_index);
}

void uniform_decrement_status_leds_mode(void) {
    if (uniform_status_leds_mode_index == 0) {
        uniform_status_leds_mode_index = uniform_status_leds_mode_count - 1;
    } 
    else {
        uniform_status_leds_mode_index--;
    }
    // any time we change modes, always invoke the mode init function
    uniform_status_led_modes[uniform_status_leds_mode_index].init();

    // update eeprom
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
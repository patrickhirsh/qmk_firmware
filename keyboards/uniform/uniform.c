// Copyright 2022 Patrick Hirsh
// SPDX-License-Identifier: GPL-2.0-or-later
#include "debug.h"
#include "print.h"

#include "uniform.h"

#include "ch.h"
#include "chtrace.h"
#include "chvt.h"
#include "hal.h"
#include "hal_pal.h"
#include "math.h"


// ======================================================================
// Status LEDs (State Info)
// ======================================================================

typedef struct {
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
} uniform_status_led_color;

static uniform_status_led_color status_leds[RGBLED_NUM];

static float uniform_status_led_brightness;
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
static uint8_t uniform_status_leds_mode_index;
static uint8_t uniform_status_leds_mode_count;
static uniform_status_led_mode uniform_status_led_modes[] = {
    (uniform_status_led_mode) { uniform_init_status_leds_sorbet, uniform_update_status_leds_sorbet },
    (uniform_status_led_mode) { uniform_init_status_leds_rainbow, uniform_update_status_leds_rainbow }
};


// ======================================================================
// Mode: Sorbet
// ======================================================================

static const float sorbet_trace_falloff_scalar = 1.5f;      // distance the light of the trace will reach
static const float sorbet_trace_speed = 35;                 // sin input (ticks) is scaled by this value to determine position (smaller value = faster)
static const uint8_t sorbet_trace_fade_time = 50;           // tick count for trace strength to fade in and out
static const uint8_t sorbet_trace_rest_pos = 70;            // should be a value between 0 and 2 * M_PI * sorbet_trace_speed (period of oscillation). trace will start here
static const float sorbet_led0_pos = -0.5;                  // led position relative to the center of the LED cluster
static const float sorbet_led1_pos = 0;                     // led position relative to the center of the LED cluster
static const float sorbet_led2_pos = 0.5;                   // led position relative to the center of the LED cluster

static uint16_t sorbet_trace_pos;
static uint8_t sorbet_trace_str;

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

// actual status led update function
void uniform_update_status_leds(void) {
    uniform_status_led_modes[uniform_status_leds_mode_index].update();
    for (int i = 0; i < RGBLED_NUM; i++) {
        rgblight_sethsv_at(
            status_leds[i].hue, 
            status_leds[i].sat, 
            uniform_status_leds_brightness_post_processing(status_leds[i].val), 
            i);
    }
}

// status led brightness post processing
const static uint8_t uniform_settings_layer_pulse_fade_time = 100;     // higher value = longer fade time when settings layer mod release
const static float uniform_settings_layer_pulse_speed = 50.0f;         // lower value = faster pulse time
const static uint16_t uniform_settings_layer_pulse_rest_pos = 60;      // should be a value between 0 and 2 * M_PI * uniform_settings_layer_pulse_speed (period of oscillation).
static uint16_t uniform_settings_layer_pulse_pos = uniform_settings_layer_pulse_rest_pos;
static uint8_t uniform_settings_layer_pulse_str = 0;
uint8_t uniform_status_leds_brightness_post_processing(uint8_t val) {

    // if we're in the settings layer or a settings layer pulse anim is still fading, update pulse anim
    if (uniform_mod_state_fn2 || uniform_settings_layer_pulse_str != 0) {

        // update pulse position
        uniform_settings_layer_pulse_pos++;
        if ((float)uniform_settings_layer_pulse_pos / uniform_settings_layer_pulse_speed > 2 * M_PI) {
            uniform_settings_layer_pulse_pos = 0;
        }
        float settings_pulse_sin_pos = sin((float)uniform_settings_layer_pulse_pos / uniform_settings_layer_pulse_speed);

        // update pulse strength based on settings layer mod key state
        if (uniform_settings_layer_pulse_str < uniform_settings_layer_pulse_fade_time && uniform_mod_state_fn2) {
            // key down, not at max... increase
            uniform_settings_layer_pulse_str++;
        } 
        if (!uniform_mod_state_fn2 && uniform_settings_layer_pulse_str != 0) {
            // key up, not at min... decrease
            uniform_settings_layer_pulse_str--;

            // bring pulse back to rest if it completely dies out
            if (uniform_settings_layer_pulse_str == 0) {
                uniform_settings_layer_pulse_pos = uniform_settings_layer_pulse_rest_pos;
            }
        }

        // linearly interpolate between base val and the val with pulse applied based on uniform_settings_layer_pulse_str
        return (((float)(uniform_settings_layer_pulse_fade_time - uniform_settings_layer_pulse_str) / (float)uniform_settings_layer_pulse_fade_time) * (float)(val) + 
                ((float)(uniform_settings_layer_pulse_str) / (float)uniform_settings_layer_pulse_fade_time) * ((float)val * ((settings_pulse_sin_pos + 1.0f) / 2.0f))) *
                uniform_status_led_brightness;
    }

    // default case - apply brightness post-processing
    return (float)val * uniform_status_led_brightness;
}

void uniform_status_leds_pulse

void uniform_increment_status_led_mode(void) {
    uniform_status_leds_mode_index++;
    if (uniform_status_leds_mode_index == uniform_status_leds_mode_count) {
        uniform_status_leds_mode_index = 0;
    }
    // any time we change modes, always invoke the mode init function
    uniform_status_led_modes[uniform_status_leds_mode_index].init();

    // update eeprom
    eeconfig_update_kb(uniform_status_leds_mode_index);
}

void uniform_decrement_status_led_mode(void) {
    if (uniform_status_leds_mode_index == 0) {
        uniform_status_leds_mode_index = uniform_status_leds_mode_count - 1;
    } 
    else {
        uniform_status_leds_mode_index--;
    }
    // any time we change modes, always invoke the mode init function
    uniform_status_led_modes[uniform_status_leds_mode_index].init();

    // update eeprom
    eeconfig_update_kb(uniform_status_leds_mode_index);
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
    uniform_status_led_brightness = UNIFORM_STATUS_LED_BRIGHTNESS > 1.0f || UNIFORM_STATUS_LED_BRIGHTNESS < 0.0f ? 1.0f : UNIFORM_STATUS_LED_BRIGHTNESS;
    
    // status led observed layer / modifier states
    uniform_mod_state_caps = false;
    uniform_mod_state_fn1 = false;
    uniform_mod_state_fn2 = false;
    
    // status led modes
    uniform_status_leds_mode_count = sizeof(uniform_status_led_modes) / sizeof(uniform_status_led_mode);
    uniform_status_leds_mode_index = eeconfig_read_kb() > uniform_status_leds_mode_count - 1 ? 0 : eeconfig_read_kb();

    // defered executor for updating status LEDs
    setPinOutput(RGB_DI_PIN);
    defer_exec(1, uniform_tick_status_leds, NULL);
}

void housekeeping_task_kb(void) {
}
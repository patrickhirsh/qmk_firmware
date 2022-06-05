// Copyright 2022 Patrick Hirsh
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include "quantum.h"

// clang-format off
#define LAYOUT_uniform( \
        k0101, k0102, k0103, k0104, k0105, k0106, k0107, k0108, k0109, k0110, k0111, k0112, k0113, k0114, \
        k0201, k0202, k0203, k0204, k0205, k0206, k0207, k0208, k0209, k0210, k0211, k0212, k0213, k0214, \
        k0301, k0302, k0303, k0304, k0305, k0306, k0307, k0308, k0309, k0310, k0311, k0312, k0313, k0314, \
        k0401, k0402, k0403, k0404, k0405, k0406, k0407, k0408, k0409, k0410, k0411, k0412, k0413, k0414, \
        k0501, k0502, k0503, k0504, k0505, k0506, k0507, k0508, k0509, k0510, k0511, k0512, k0513, k0514 \
    ) \
    { \
        { k0101, k0102, k0103, k0104, k0105, k0106, k0107, k0108, k0109, k0110, k0111, k0112, k0113, k0114 }, \
        { k0201, k0202, k0203, k0204, k0205, k0206, k0207, k0208, k0209, k0210, k0211, k0212, k0213, k0214 }, \
        { k0301, k0302, k0303, k0304, k0305, k0306, k0307, k0308, k0309, k0310, k0311, k0312, k0313, k0314 }, \
        { k0401, k0402, k0403, k0404, k0405, k0406, k0407, k0408, k0409, k0410, k0411, k0412, k0413, k0414 }, \
        { k0501, k0502, k0503, k0504, k0505, k0506, k0507, k0508, k0509, k0510, k0511, k0512, k0513, k0514 }  \
    }
// clang-format on

void exec_bootloader(void);

// Status LED state information
void uniform_increment_status_led_mode(void);
void uniform_decrement_status_led_mode(void);
void uniform_flip_mod_state_caps(void);
void uniform_set_mod_state_fn1(bool state);
void uniform_set_mod_state_fn2(bool state);

// Status LEDs Tick
uint32_t uniform_tick_status_leds(uint32_t trigger_time, void* cb_arg);
void uniform_update_status_leds(void);
uint8_t uniform_status_leds_brightness_post_processing(uint8_t val);

// Status LEDs (Modes)
void uniform_init_status_leds_sorbet(void);
void uniform_update_status_leds_sorbet(void);
void uniform_init_status_leds_rainbow(void);
void uniform_update_status_leds_rainbow(void);
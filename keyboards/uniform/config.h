// Copyright 2022 Patrick Hirsh (@Patrick Hirsh)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* Bootloader */
#define EARLY_INIT_PERFORM_BOOTLOADER_JUMP TRUE

/* USB Device descriptor parameter */
#define VENDOR_ID 0xFEED
#define PRODUCT_ID 0x9797
#define DEVICE_VER 0x0001
#define MANUFACTURER Patrick Hirsh
#define PRODUCT Uniform Keyboard
#define DESCRIPTION Uniform Keyboard

/* key matrix */
#define DIODE_DIRECTION COL2ROW
#define MATRIX_ROWS 5
#define MATRIX_COLS 14
#define MATRIX_ROW_PINS { B4, B5, B7, C15, C14 }
#define MATRIX_COL_PINS { A0, A1, A2, A3, A4, A5, A6, A7, B0, B1, A8, A9, A10, B6 }

/* Status RGB LED */
#define RGB_DI_PIN A15
#define RGBLED_NUM 3
#define UNIFORM_STATUS_LED_TICKRATE 8               // ms (16 = 62.5 ticks/s, 8 = 125 ticks/s)
#define WS2812_BYTE_ORDER WS2812_BYTE_ORDER_RGB

/* Set 0 if debouncing isn't needed */
#define DEBOUNCE 5

/* ChibiOS hooks to reroute errors to QMK toolbox */
#define chDbgCheck(c)                                                                                   \
    do {                                                                                                \
        if (CH_DBG_ENABLE_CHECKS != FALSE) {                                                            \
            if (!(c)) {                                                                                 \
                extern void chibi_debug_check_hook(const char* func, const char* condition, int value); \
                chibi_debug_check_hook(__func__, (#c), (c));                                            \
                chSysHalt(__func__);                                                                    \
            }                                                                                           \
        }                                                                                               \
    } while (false)

#define chDbgAssert(c, r)                                                                                                    \
    do {                                                                                                                     \
        if (CH_DBG_ENABLE_ASSERTS != FALSE) {                                                                                \
            if (!(c)) {                                                                                                      \
                extern void chibi_debug_assert_hook(const char* func, const char* condition, int value, const char* reason); \
                chibi_debug_assert_hook(__func__, (#c), (c), (r));                                                           \
                chSysHalt(__func__);                                                                                         \
            }                                                                                                                \
        }                                                                                                                    \
    } while (false)

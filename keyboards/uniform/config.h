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

/* key matrix size */
#define MATRIX_ROWS 5
#define MATRIX_COLS 14

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

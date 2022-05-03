#include QMK_KEYBOARD_H

enum layers {
    UNI_DEFAULT,
    UNI_FN1,
};

enum custom_keycodes {
  KC_UNI_FN1 = SAFE_RANGE,
};


const uint16_t keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [UNI_DEFAULT] = LAYOUT_uniform(
        KC_ESC,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,
        KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_RBRC,    KC_BSLS,
        KC_CAPS,    KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_QUOT,    KC_ENT,     KC_HOME,
        KC_LSFT,    KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,    KC_RSFT,    KC_UP,      KC_END,
        KC_LCTL,    KC_LWIN,    KC_LALT,    KC_NO,      KC_SPC,     KC_UNI_FN1, KC_SPC,     KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_LEFT,    KC_DOWN,    KC_RGHT
    ),
    [UNI_FN1] = LAYOUT_uniform(
        KC_GRAVE,   KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,     KC_F11,     KC_F12,     KC_DEL,
        KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_7,       KC_8,       KC_9,       KC_LBRC,    KC_RBRC,    KC_BSLS,
        KC_CAPS,    KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_4,       KC_5,       KC_6,       KC_QUOT,    KC_ENT,                                         KC_AUDIO_VOL_UP,
        KC_LSFT,    KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_1,       KC_2,       KC_3,       KC_RSFT,                            KC_AUDIO_MUTE,          KC_AUDIO_VOL_DOWN,
        KC_LCTL,    KC_LWIN,    KC_LALT,    KC_NO,      KC_SPC,     KC_UNI_FN1, KC_SPC,     KC_NO,      KC_0,       KC_0,       KC_DOT,                 KC_MEDIA_PREV_TRACK,    KC_MEDIA_PLAY_PAUSE,    KC_MEDIA_NEXT_TRACK
    )
};

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  switch (keycode) {
      case KC_UNI_FN1:
      if (record->event.pressed) {
        layer_on(UNI_FN1);
      } else {
        layer_off(UNI_FN1);
      }
      return false;
      break;
  }
  return true;
}
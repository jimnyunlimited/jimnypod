#ifndef SETTINGS_APP_H
#define SETTINGS_APP_H

#include <Arduino.h>
#include "lvgl.h"

// --- Settings Globals ---
extern int current_brightness;
extern lv_obj_t * settings_screen;
extern lv_obj_t * brightness_label;
extern lv_obj_t * brightness_overlay;

// --- Function Prototypes ---
void build_settings_screen();
void switch_to_settings();
void set_brightness(int value);
void setup_brightness_overlay();

#endif // SETTINGS_APP_H

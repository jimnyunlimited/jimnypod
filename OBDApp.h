#ifndef OBD_APP_H
#define OBD_APP_H

#include <Arduino.h>
#include "lvgl.h"

// --- OBD Globals ---
extern lv_obj_t * obd_screen;
extern lv_obj_t * obd_screen2;

// --- Function Prototypes ---
void build_obd_screen();
void build_obd_screen2();
void switch_to_obd();
void switch_to_obd2();
void obd_setup();
void obd_loop_handler();

// WiFi Management
void start_obd_wifi();
void stop_obd_wifi();

#endif // OBD_APP_H

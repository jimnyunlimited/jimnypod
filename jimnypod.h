#ifndef JIMNYPOD_H
#define JIMNYPOD_H

#include <Arduino.h>
#include "lvgl.h"

// --- Global State Machine ---
enum AppState { STATE_LAUNCHER, STATE_INCLINOMETER, STATE_PHOTOFRAME, STATE_SETTINGS };
extern AppState current_state;

// --- Function Prototypes ---
void switch_to_launcher();
void switch_to_inclinometer();
void switch_to_photoframe();
void switch_to_settings();

#endif // JIMNYPOD_H

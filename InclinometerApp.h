#ifndef INCLINOMETER_APP_H
#define INCLINOMETER_APP_H

#include <Arduino.h>
#include "lvgl.h"
#include "qmi8658c.h"

// Inclinometer Globals
extern lv_obj_t * inclinometer_screen;
extern lv_obj_t * ground_line;
extern lv_point_t ground_points[2];
#define LADDER_COUNT 13
extern lv_obj_t * ladder_lines[LADDER_COUNT];
extern lv_point_t ladder_points[LADDER_COUNT][2];
extern lv_obj_t * ladder_labels_l[LADDER_COUNT];
extern lv_obj_t * ladder_labels_r[LADDER_COUNT];
#define ROLL_MARKER_COUNT 19
extern lv_obj_t * roll_markers[ROLL_MARKER_COUNT];
extern lv_point_t roll_marker_points[ROLL_MARKER_COUNT][3];
extern lv_obj_t * roll_pointer_line;
extern lv_point_t roll_pointer_points[3];

struct LadderLine { float x1, x2, y; int degree; };
extern const LadderLine ladder_base[LADDER_COUNT];
extern lv_point_t aircraft_w_points[5];

extern float pitch, roll, pitch_offset, roll_offset;
#define ALPHA 0.85
extern unsigned long last_time;
extern bool imu_ready;

// Function Prototypes
void build_inclinometer_screen();
void updateAttitude();
void updateUI();
void switch_to_inclinometer();
void inclinometer_setup_imu();
void inclinometer_loop_handler();

#endif // INCLINOMETER_APP_H

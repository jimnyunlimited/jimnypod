#include "InclinometerApp.h"
#include "jimnypod.h"
#include <math.h>

// Inclinometer Globals (Definitions)
lv_obj_t * inclinometer_screen = NULL;
lv_obj_t * ground_line;
lv_point_t ground_points[2];
lv_obj_t * ladder_lines[LADDER_COUNT];
lv_point_t ladder_points[LADDER_COUNT][2];
lv_obj_t * ladder_labels_l[LADDER_COUNT];
lv_obj_t * ladder_labels_r[LADDER_COUNT];
lv_obj_t * roll_markers[ROLL_MARKER_COUNT];
lv_point_t roll_marker_points[ROLL_MARKER_COUNT][3];
lv_obj_t * roll_pointer_line;
lv_point_t roll_pointer_points[3];

const LadderLine ladder_base[LADDER_COUNT] = {
    {-100, 100, 0,    0}, {-50,  50,  -60,  10}, {-70,  70,  -120, 20},
    {-50,  50,  -180, 30}, {-70,  70,  -240, 40}, {-50,  50,  -300, 50},
    {-70,  70,  -360, 60}, {-50,  50,  60,  -10}, {-70,  70,  120, -20},
    {-50,  50,  180, -30}, {-70,  70,  240, -40}, {-50,  50,  300, -50},
    {-70,  70,  360, -60}
};
lv_point_t aircraft_w_points[5] = {
    {233 - 120, 233}, {233 - 40,  233}, {233, 233 + 30}, {233 + 40,  233}, {233 + 120, 233}
};
float pitch = 0.0, roll = 0.0, pitch_offset = 0.0, roll_offset = 0.0;
unsigned long last_time = 0;
bool imu_ready = false;

// Callbacks
static void inclinometer_gesture_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SHORT_CLICKED) {
        // Quick tap to zero
        pitch_offset = pitch;
        roll_offset = roll;
        Serial.println("Sensors Zeroed!");
    }
    else if (code == LV_EVENT_LONG_PRESSED) {
        // Hold finger for 1 second to exit
        switch_to_launcher();
    }
}

// Screen Builder
void build_inclinometer_screen() {
    inclinometer_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(inclinometer_screen, lv_color_hex(0x2B82CB), 0); 
    lv_obj_clear_flag(inclinometer_screen, LV_OBJ_FLAG_SCROLLABLE);

    ground_line = lv_line_create(inclinometer_screen);
    lv_obj_set_style_line_width(ground_line, 1200, 0);
    lv_obj_set_style_line_color(ground_line, lv_color_hex(0x8B4513), 0);
    lv_obj_set_style_line_rounded(ground_line, false, 0);
    lv_obj_clear_flag(ground_line, LV_OBJ_FLAG_CLICKABLE);

    for(int i=0; i<LADDER_COUNT; i++) {
        ladder_lines[i] = lv_line_create(inclinometer_screen);
        lv_obj_set_style_line_width(ladder_lines[i], 3, 0);
        lv_obj_set_style_line_color(ladder_lines[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_line_rounded(ladder_lines[i], true, 0);
        lv_obj_clear_flag(ladder_lines[i], LV_OBJ_FLAG_CLICKABLE);

        if (ladder_base[i].degree != 0) {
            ladder_labels_l[i] = lv_label_create(inclinometer_screen);
            lv_label_set_text_fmt(ladder_labels_l[i], "%d", abs(ladder_base[i].degree));
            lv_obj_set_style_text_color(ladder_labels_l[i], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(ladder_labels_l[i], &lv_font_montserrat_24, 0);
            lv_obj_clear_flag(ladder_labels_l[i], LV_OBJ_FLAG_CLICKABLE);

            ladder_labels_r[i] = lv_label_create(inclinometer_screen);
            lv_label_set_text_fmt(ladder_labels_r[i], "%d", abs(ladder_base[i].degree));
            lv_obj_set_style_text_color(ladder_labels_r[i], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(ladder_labels_r[i], &lv_font_montserrat_24, 0);
            lv_obj_clear_flag(ladder_labels_r[i], LV_OBJ_FLAG_CLICKABLE);
        } else {
            ladder_labels_l[i] = NULL;
            ladder_labels_r[i] = NULL;
        }
    }

    lv_obj_t * roll_ring = lv_obj_create(inclinometer_screen);
    lv_obj_set_size(roll_ring, 466, 466);
    lv_obj_align(roll_ring, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(roll_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(roll_ring, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(roll_ring, 24, 0); 
    lv_obj_set_style_radius(roll_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(roll_ring, LV_OBJ_FLAG_CLICKABLE);

    for(int i = 0; i < ROLL_MARKER_COUNT; i++) {
        int angle_deg = -90 + (i * 10);
        if (angle_deg != 0 && abs(angle_deg) % 30 == 0) {
            roll_markers[i] = lv_obj_create(inclinometer_screen);
            lv_obj_set_size(roll_markers[i], 24, 24); 
            lv_obj_set_style_bg_color(roll_markers[i], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_opa(roll_markers[i], LV_OPA_COVER, 0);
            lv_obj_set_style_radius(roll_markers[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(roll_markers[i], 0, 0);
            lv_obj_clear_flag(roll_markers[i], LV_OBJ_FLAG_CLICKABLE);
        } else {
            roll_markers[i] = lv_line_create(inclinometer_screen);
            lv_obj_set_style_line_color(roll_markers[i], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_line_rounded(roll_markers[i], true, 0);
            lv_obj_clear_flag(roll_markers[i], LV_OBJ_FLAG_CLICKABLE);
        }
    }

    roll_pointer_line = lv_line_create(inclinometer_screen);
    lv_obj_set_style_line_width(roll_pointer_line, 4, 0);
    lv_obj_set_style_line_color(roll_pointer_line, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_line_rounded(roll_pointer_line, true, 0);
    lv_obj_clear_flag(roll_pointer_line, LV_OBJ_FLAG_CLICKABLE);
    roll_pointer_points[0].x = 233 - 14; roll_pointer_points[0].y = 233 - 185;
    roll_pointer_points[1].x = 233;      roll_pointer_points[1].y = 233 - 205; 
    roll_pointer_points[2].x = 233 + 14; roll_pointer_points[2].y = 233 - 185;
    lv_line_set_points(roll_pointer_line, roll_pointer_points, 3);

    lv_obj_t * aircraft_w = lv_line_create(inclinometer_screen);
    lv_line_set_points(aircraft_w, aircraft_w_points, 5);
    lv_obj_set_style_line_width(aircraft_w, 6, 0);
    lv_obj_set_style_line_color(aircraft_w, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_line_rounded(aircraft_w, true, 0);
    lv_obj_clear_flag(aircraft_w, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * center_dot = lv_obj_create(inclinometer_screen);
    lv_obj_set_size(center_dot, 10, 10);
    lv_obj_align(center_dot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(center_dot, lv_color_hex(0xFFFF00), 0); 
    lv_obj_set_style_bg_opa(center_dot, LV_OPA_COVER, 0); 
    lv_obj_set_style_border_width(center_dot, 0, 0);
    lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(center_dot, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * touch_overlay = lv_obj_create(inclinometer_screen);
    lv_obj_set_size(touch_overlay, 466, 466);
    lv_obj_align(touch_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(touch_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(touch_overlay, 0, 0);
    lv_obj_clear_flag(touch_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(touch_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(touch_overlay, inclinometer_gesture_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(touch_overlay, inclinometer_gesture_cb, LV_EVENT_LONG_PRESSED, NULL);

    updateUI();    
}

// Inclinometer Functions
void updateAttitude() {
    float accel[3] = {0.0f, 0.0f, 0.0f};
    float gyro[3] = {0.0f, 0.0f, 0.0f};
    qmi8658_read_xyz(accel, gyro);
    unsigned long current_time = millis();
    float dt = (current_time - last_time) / 1000.0;
    if (dt > 0.1 || dt <= 0.0) dt = 0.01;
    last_time = current_time;
    if (abs(gyro[0]) < 1.0) gyro[0] = 0.0;
    if (abs(gyro[1]) < 1.0) gyro[1] = 0.0;
    float accel_pitch = atan2(-accel[0], accel[2]) * 180.0 / PI;
    float accel_roll = atan2(-accel[1], sqrt(accel[0]*accel[0] + accel[2]*accel[2])) * 180.0 / PI;
    pitch += gyro[1] * dt;
    roll -= gyro[0] * dt; 
    if (pitch - accel_pitch > 180.0) pitch -= 360.0;
    else if (pitch - accel_pitch < -180.0) pitch += 360.0;
    if (roll - accel_roll > 180.0) roll -= 360.0;
    else if (roll - accel_roll < -180.0) roll += 360.0;
    pitch = ALPHA * pitch + (1.0 - ALPHA) * accel_pitch;
    roll = ALPHA * roll + (1.0 - ALPHA) * accel_roll;
    while (pitch > 180.0) pitch -= 360.0;
    while (pitch < -180.0) pitch += 360.0;
    while (roll > 180.0) roll -= 360.0;
    while (roll < -180.0) roll += 360.0;
    if (isnan(pitch) || isinf(pitch)) pitch = 0.0;
    if (isnan(roll) || isinf(roll)) roll = 0.0;
}

void updateUI() {
    float display_pitch = (pitch - pitch_offset);
    float display_roll = (roll - roll_offset);
    while (display_pitch > 180.0) display_pitch -= 360.0;
    while (display_pitch < -180.0) display_pitch += 360.0;
    while (display_roll > 180.0) display_roll -= 360.0;
    while (display_roll < -180.0) display_roll += 360.0;
    if (display_pitch > 60.0) display_pitch = 60.0;
    if (display_pitch < -60.0) display_pitch = -60.0;
    if (display_roll > 90.0) display_roll = 90.0;
    if (display_roll < -90.0) display_roll = -90.0;
    int pitch_shift = (int)(display_pitch * 6.0);
    if (pitch_shift > 400) pitch_shift = 400;
    if (pitch_shift < -400) pitch_shift = -400;
    float angle_rad = -display_roll * PI / 180.0;
    float cos_a = cos(angle_rad);
    float sin_a = sin(angle_rad);
    int CX = 233; 
    int CY = 233;

    float gx1 = -1000.0; float gy1 = 600.0 + pitch_shift;
    float gx2 = 1000.0;  float gy2 = 600.0 + pitch_shift;
    ground_points[0].x = CX + (int)(gx1 * cos_a - gy1 * sin_a);
    ground_points[0].y = CY + (int)(gx1 * sin_a + gy1 * cos_a);
    ground_points[1].x = CX + (int)(gx2 * cos_a - gy2 * sin_a);
    ground_points[1].y = CY + (int)(gx2 * sin_a + gy2 * cos_a);
    lv_line_set_points(ground_line, ground_points, 2);

    for(int i = 0; i < LADDER_COUNT; i++) {
        float lx1 = ladder_base[i].x1;
        float lx2 = ladder_base[i].x2;
        float ly = ladder_base[i].y + pitch_shift;
        ladder_points[i][0].x = CX + (int)lx1;
        ladder_points[i][0].y = CY + (int)ly;
        ladder_points[i][1].x = CX + (int)lx2;
        ladder_points[i][1].y = CY + (int)ly;
        lv_line_set_points(ladder_lines[i], ladder_points[i], 2);
        if (ladder_base[i].degree != 0) {
            float llx = lx1 - 30;
            lv_obj_set_pos(ladder_labels_l[i], CX + (int)llx - 15, CY + (int)ly - 12);
            float rlx = lx2 + 30;
            lv_obj_set_pos(ladder_labels_r[i], CX + (int)rlx - 5, CY + (int)ly - 12);
        }
    }

    for(int i = 0; i < ROLL_MARKER_COUNT; i++) {
        int angle_deg = -90 + (i * 10); 
        float marker_angle_rad = (angle_deg - 90 - display_roll) * PI / 180.0;
        if (angle_deg != 0 && abs(angle_deg) % 30 == 0) {
            int r = 221; 
            int cx = CX + (int)(r * cos(marker_angle_rad));
            int cy = CY + (int)(r * sin(marker_angle_rad));
            lv_obj_set_pos(roll_markers[i], cx - 12, cy - 12); 
        } 
        else if (angle_deg == 0) {
            lv_obj_set_style_line_width(roll_markers[i], 4, 0);
            float delta = 0.06; 
            roll_marker_points[i][0].x = CX + (int)(233 * cos(marker_angle_rad - delta));
            roll_marker_points[i][0].y = CY + (int)(233 * sin(marker_angle_rad - delta));
            roll_marker_points[i][1].x = CX + (int)(209 * cos(marker_angle_rad)); 
            roll_marker_points[i][1].y = CY + (int)(209 * sin(marker_angle_rad));
            roll_marker_points[i][2].x = CX + (int)(233 * cos(marker_angle_rad + delta));
            roll_marker_points[i][2].y = CY + (int)(233 * sin(marker_angle_rad + delta));
            lv_line_set_points(roll_markers[i], roll_marker_points[i], 3);
        } 
        else {
            lv_obj_set_style_line_width(roll_markers[i], 3, 0);
            int inner_r = 217;
            roll_marker_points[i][0].x = CX + (int)(233 * cos(marker_angle_rad));
            roll_marker_points[i][0].y = CY + (int)(233 * sin(marker_angle_rad));
            roll_marker_points[i][1].x = CX + (int)(inner_r * cos(marker_angle_rad));
            roll_marker_points[i][1].y = CY + (int)(inner_r * sin(marker_angle_rad));
            lv_line_set_points(roll_markers[i], roll_marker_points[i], 2);
        }
    }
}

void switch_to_inclinometer() {
    current_state = STATE_INCLINOMETER;
    lv_scr_load_anim(inclinometer_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
}

void inclinometer_setup_imu() {
    Serial.println("Initializing QMI8658 IMU...");
    if (qmi8658_init()) {
        qmi8658_config_reg(0);
        qmi8658_enableSensors(QMI8658_ACCGYR_ENABLE);
        for (int i = 0; i < 50; i++) {
            float acc[3], gyro[3];
            qmi8658_read_xyz(acc, gyro);
            delay(20);
        }
        imu_ready = true;
        Serial.println("✓ IMU ready");
    } else {
        Serial.println("⚠ IMU initialization failed!");
    }
}

void inclinometer_loop_handler() {
    static unsigned long last_update = 0;
    if (millis() - last_update >= 20) {
        if (imu_ready) {
            updateAttitude();
        }
        // Always update UI so it doesn't stay blank even if IMU is disconnected
        updateUI();
        last_update = millis();
    }
}

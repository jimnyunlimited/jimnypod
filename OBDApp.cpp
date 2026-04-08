#include "OBDApp.h"
#include "jimnypod.h"
#include "lcd_bsp.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <math.h>

// --- SIMULATION TOGGLE ---
#define SIMULATE_OBD 1

// Massive 200px font
LV_FONT_DECLARE(ui_font_rajdhani200);

// --- OBD Globals ---
lv_obj_t * obd_screen = NULL;
lv_obj_t * obd_screen2 = NULL;
lv_obj_t * rpm_meter = NULL;
lv_meter_indicator_t * rpm_indicator = NULL; 
lv_obj_t * speed_label = NULL;

// Screen 2 Globals
lv_obj_t * gear_label = NULL;
lv_obj_t * temp_arc2 = NULL;
lv_obj_t * temp_val_label = NULL;
lv_obj_t * torque_arc = NULL;
lv_obj_t * torque_val_label = NULL;
lv_obj_t * battery_label2 = NULL;

volatile int car_rpm = 0;
volatile int car_speed = 0;
volatile int car_engine_temp = 90;
volatile float car_voltage = 14.1;
volatile int car_torque = 0;
char car_gear[4] = "N";

const char* obd_ssid = "WiFi_OBDII"; 
const char* obd_ip = "192.168.0.10";
const uint16_t obd_port = 35000;

static bool obd_wifi_running = false;

// --- Background Worker with Gear Shifts and Torque Simulation ---
void obdBackgroundWorker(void *pvParameters) {
#if SIMULATE_OBD
    float sim_speed = 0;
    float sim_rpm = 800;
    int gear_num = 1;
    bool accelerating = true;

    while(1) {
        if (accelerating) {
            sim_speed += 0.4;
            sim_rpm += (65.0 / gear_num); 
            
            float torque_factor = 1.0 - (abs(sim_rpm - 4000.0) / 4000.0);
            if (torque_factor < 0.2) torque_factor = 0.2;
            car_torque = (int)(130.0 * torque_factor);

            if (sim_rpm >= 6200) {
                if (gear_num < 5) {
                    gear_num++;
                    sim_rpm = 3400;
                    vTaskDelay(pdMS_TO_TICKS(250));
                } else { accelerating = false; }
            }
            if (sim_speed > 175) accelerating = false;
        } else {
            sim_speed -= 0.8;
            sim_rpm -= 60;
            car_torque = 10;
            if (sim_speed <= 0) {
                sim_speed = 0; sim_rpm = 800; gear_num = 1;
                accelerating = true;
                vTaskDelay(pdMS_TO_TICKS(1500)); 
            }
        }
        
        car_speed = (int)sim_speed;
        car_rpm = (int)sim_rpm;
        
        if (car_speed == 0) strcpy(car_gear, "N");
        else sprintf(car_gear, "D%d", gear_num);

        car_engine_temp = 90 + (int)(sin(millis()/10000.0) * 5);
        car_voltage = 13.8 + (sin(millis()/5000.0) * 0.5);

        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
#else
    // Real OBD Logic...
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif
}

// --- Callbacks ---
static void obd_gesture_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_LONG_PRESSED) {
        Serial.println("OBD: Exit Requested");
        stop_obd_wifi();
        switch_to_launcher();
    } 
    else if (code == LV_EVENT_SHORT_CLICKED) {
        Serial.println("OBD: Tap to Cycle");
        if (lv_scr_act() == obd_screen) {
            switch_to_obd2();
        } else {
            switch_to_obd();
        }
    }
}

void build_obd_screen() {
    obd_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(obd_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(obd_screen, LV_OPA_COVER, 0); 
    lv_obj_clear_flag(obd_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Tachometer Base
    rpm_meter = lv_meter_create(obd_screen);
    lv_obj_set_size(rpm_meter, 466, 466);
    lv_obj_center(rpm_meter);
    lv_obj_set_style_bg_opa(rpm_meter, 0, 0);
    lv_obj_set_style_border_width(rpm_meter, 0, 0);
    // Remove center dot (knob)
    lv_obj_set_style_bg_opa(rpm_meter, 0, LV_PART_INDICATOR);
    lv_obj_clear_flag(rpm_meter, LV_OBJ_FLAG_CLICKABLE);

    lv_meter_scale_t * scale = lv_meter_add_scale(rpm_meter);
    lv_meter_set_scale_range(rpm_meter, scale, 0, 8000, 270, 135);
    lv_meter_set_scale_ticks(rpm_meter, scale, 41, 2, 12, lv_color_hex(0xFFFFFF)); 
    // NO BUILT-IN LABELS (length = 0)
    lv_meter_set_scale_major_ticks(rpm_meter, scale, 5, 4, 22, lv_color_hex(0xFFFFFF), 0); 

    // Manual labels 1-8 at r = 175
    for(int i=1; i<=8; i++) {
        lv_obj_t * lbl = lv_label_create(obd_screen);
        lv_label_set_text_fmt(lbl, "%d", i);
        float angle_deg = 135.0f + (i * 33.75f); 
        float angle_rad = angle_deg * M_PI / 180.0f;
        int r = 175; 
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(lbl, (i >= 6) ? lv_color_hex(0xFF0000) : lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, (int)(r * cos(angle_rad)), (int)(r * sin(angle_rad)));
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_meter_indicator_t * red_arc = lv_meter_add_arc(rpm_meter, scale, 15, lv_color_hex(0xFF0000), 0);
    lv_meter_set_indicator_start_value(rpm_meter, red_arc, 6000);
    lv_meter_set_indicator_end_value(rpm_meter, red_arc, 8000);

    rpm_indicator = lv_meter_add_arc(rpm_meter, scale, 15, lv_color_hex(0xE67E22), 0);
    lv_meter_set_indicator_end_value(rpm_meter, rpm_indicator, 0);

    speed_label = lv_label_create(obd_screen);
    lv_label_set_text(speed_label, "0");
    lv_obj_set_style_text_color(speed_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(speed_label, &ui_font_rajdhani200, 0);
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, -10);
    lv_obj_clear_flag(speed_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * unit_lbl = lv_label_create(obd_screen);
    lv_label_set_text(unit_lbl, "km/h");
    lv_obj_set_style_text_color(unit_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(unit_lbl, &lv_font_montserrat_28, 0);
    lv_obj_align(unit_lbl, LV_ALIGN_CENTER, 0, 85);
    lv_obj_clear_flag(unit_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * rpm_unit_lbl = lv_label_create(obd_screen);
    lv_label_set_text(rpm_unit_lbl, "x 1000 rpm");
    lv_obj_set_style_text_color(rpm_unit_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(rpm_unit_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(rpm_unit_lbl, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_clear_flag(rpm_unit_lbl, LV_OBJ_FLAG_CLICKABLE);

    // INVISIBLE INPUT OVERLAY - Top Layer
    lv_obj_t * input_layer = lv_obj_create(obd_screen);
    lv_obj_set_size(input_layer, 466, 466);
    lv_obj_set_style_bg_opa(input_layer, 0, 0);
    lv_obj_set_style_border_width(input_layer, 0, 0);
    lv_obj_center(input_layer);
    lv_obj_add_flag(input_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(input_layer, obd_gesture_cb, LV_EVENT_ALL, NULL);
}

void build_obd_screen2() {
    obd_screen2 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(obd_screen2, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(obd_screen2, LV_OPA_COVER, 0); 
    lv_obj_clear_flag(obd_screen2, LV_OBJ_FLAG_SCROLLABLE);

    gear_label = lv_label_create(obd_screen2);
    lv_label_set_text(gear_label, "N");
    lv_obj_set_style_text_color(gear_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(gear_label, &ui_font_rajdhani200, 0);
    lv_obj_align(gear_label, LV_ALIGN_CENTER, 0, -10);
    lv_obj_clear_flag(gear_label, LV_OBJ_FLAG_CLICKABLE);

    temp_arc2 = lv_arc_create(obd_screen2);
    lv_obj_set_size(temp_arc2, 440, 440);
    lv_obj_center(temp_arc2);
    lv_arc_set_range(temp_arc2, 40, 120);
    lv_arc_set_bg_angles(temp_arc2, 120, 240); 
    lv_arc_set_value(temp_arc2, 90);
    lv_obj_set_style_arc_width(temp_arc2, 15, LV_PART_MAIN);
    lv_obj_set_style_arc_width(temp_arc2, 15, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(temp_arc2, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(temp_arc2, lv_color_hex(0x3498DB), LV_PART_INDICATOR);
    lv_obj_remove_style(temp_arc2, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(temp_arc2, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * temp_icon = lv_label_create(obd_screen2);
    lv_label_set_text(temp_icon, LV_SYMBOL_TINT);
    lv_obj_set_style_text_color(temp_icon, lv_color_hex(0x3498DB), 0);
    lv_obj_set_style_text_font(temp_icon, &lv_font_montserrat_28, 0);
    lv_obj_align(temp_icon, LV_ALIGN_CENTER, -160, -40);

    temp_val_label = lv_label_create(obd_screen2);
    lv_label_set_text(temp_val_label, "90°C");
    lv_obj_set_style_text_color(temp_val_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(temp_val_label, &lv_font_montserrat_24, 0);
    lv_obj_align(temp_val_label, LV_ALIGN_CENTER, -160, 10);

    torque_arc = lv_arc_create(obd_screen2);
    lv_obj_set_size(torque_arc, 440, 440);
    lv_obj_center(torque_arc);
    lv_arc_set_range(torque_arc, 0, 150);
    lv_arc_set_bg_angles(torque_arc, 300, 60); 
    lv_arc_set_value(torque_arc, 0);
    lv_obj_set_style_arc_width(torque_arc, 15, LV_PART_MAIN);
    lv_obj_set_style_arc_width(torque_arc, 15, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(torque_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(torque_arc, lv_color_hex(0xE67E22), LV_PART_INDICATOR);
    lv_obj_remove_style(torque_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(torque_arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * torque_icon = lv_label_create(obd_screen2);
    lv_label_set_text(torque_icon, LV_SYMBOL_SETTINGS); 
    lv_obj_set_style_text_color(torque_icon, lv_color_hex(0xE67E22), 0);
    lv_obj_set_style_text_font(torque_icon, &lv_font_montserrat_28, 0);
    lv_obj_align(torque_icon, LV_ALIGN_CENTER, 160, -40);

    torque_val_label = lv_label_create(obd_screen2);
    lv_label_set_text(torque_val_label, "0 NM");
    lv_obj_set_style_text_color(torque_val_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(torque_val_label, &lv_font_montserrat_24, 0);
    lv_obj_align(torque_val_label, LV_ALIGN_CENTER, 160, 10);

    lv_obj_t * batt_icon = lv_label_create(obd_screen2);
    lv_label_set_text(batt_icon, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(batt_icon, lv_color_hex(0x2ECC71), 0);
    lv_obj_set_style_text_font(batt_icon, &lv_font_montserrat_28, 0);
    lv_obj_align(batt_icon, LV_ALIGN_BOTTOM_MID, -40, -60);

    battery_label2 = lv_label_create(obd_screen2);
    lv_label_set_text(battery_label2, "14.1V");
    lv_obj_set_style_text_color(battery_label2, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(battery_label2, &lv_font_montserrat_28, 0);
    lv_obj_align(battery_label2, LV_ALIGN_BOTTOM_MID, 30, -60);

    // INVISIBLE INPUT OVERLAY - Screen 2
    lv_obj_t * input_layer2 = lv_obj_create(obd_screen2);
    lv_obj_set_size(input_layer2, 466, 466);
    lv_obj_set_style_bg_opa(input_layer2, 0, 0);
    lv_obj_set_style_border_width(input_layer2, 0, 0);
    lv_obj_center(input_layer2);
    lv_obj_add_flag(input_layer2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(input_layer2, obd_gesture_cb, LV_EVENT_ALL, NULL);
}

void switch_to_obd() {
    current_state = STATE_OBD;
    lv_scr_load_anim(obd_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
    start_obd_wifi();
}

void switch_to_obd2() {
    current_state = STATE_OBD; 
    lv_scr_load_anim(obd_screen2, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

void obd_setup() {
    xTaskCreatePinnedToCore(obdBackgroundWorker, "OBD_Task", 8192, NULL, 1, NULL, 0);
}

void start_obd_wifi() {
#if SIMULATE_OBD
    obd_wifi_running = true; return;
#endif
    if (obd_wifi_running) return;
    WiFi.mode(WIFI_STA); WiFi.begin(obd_ssid);
    obd_wifi_running = true;
}

void stop_obd_wifi() {
    if (!obd_wifi_running) return;
#if !SIMULATE_OBD
    WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
#endif
    obd_wifi_running = false;
}

void obd_loop_handler() {
    if (current_state != STATE_OBD) return;
    static int last_s = -1, last_r = -1, last_t = -1, last_trq = -1;
    static float last_v = -1.0;
    static char last_g[4] = "";
    static uint32_t last_ui_update = 0;

    if (millis() - last_ui_update < 100) return;
    last_ui_update = millis();

    if (lv_scr_act() == obd_screen) {
        if (car_rpm != last_r) {
            lv_meter_set_indicator_end_value(rpm_meter, rpm_indicator, car_rpm);
            last_r = car_rpm;
        }
        if (car_speed != last_s) {
            lv_label_set_text_fmt(speed_label, "%d", car_speed);
            last_s = car_speed;
        }
    }

    if (lv_scr_act() == obd_screen2) {
        if (strcmp(car_gear, last_g) != 0) {
            lv_label_set_text(gear_label, car_gear);
            strcpy(last_g, car_gear);
        }
        if (car_engine_temp != last_t) {
            lv_arc_set_value(temp_arc2, car_engine_temp);
            lv_label_set_text_fmt(temp_val_label, "%d°C", car_engine_temp);
            last_t = car_engine_temp;
        }
        if (car_torque != last_trq) {
            lv_arc_set_value(torque_arc, car_torque);
            lv_label_set_text_fmt(torque_val_label, "%d NM", car_torque);
            last_trq = car_torque;
        }
        if (abs(car_voltage - last_v) > 0.05) {
            int v_whole = (int)car_voltage;
            int v_dec = (int)(car_voltage * 10) % 10;
            lv_label_set_text_fmt(battery_label2, "%d.%dV", v_whole, v_dec);
            last_v = car_voltage;
        }
    }

    lv_obj_invalidate(lv_scr_act());
}

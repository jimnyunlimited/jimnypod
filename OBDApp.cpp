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
lv_obj_t * rpm_meter = NULL;
lv_meter_indicator_t * rpm_indicator = NULL; 
lv_obj_t * speed_label = NULL;

volatile int car_rpm = 0;
volatile int car_speed = 0;

const char* obd_ssid = "WiFi_OBDII"; 
const char* obd_ip = "192.168.0.10";
const uint16_t obd_port = 35000;

static bool obd_wifi_running = false;

// --- Background Worker with Gear Shifts ---
void obdBackgroundWorker(void *pvParameters) {
#if SIMULATE_OBD
    float sim_speed = 0;
    float sim_rpm = 800;
    int gear = 1;
    bool accelerating = true;

    while(1) {
        if (accelerating) {
            sim_speed += 0.4;
            sim_rpm += (65.0 / gear); 
            if (sim_rpm >= 6200) {
                if (gear < 5) {
                    gear++; sim_rpm = 3400;
                    vTaskDelay(pdMS_TO_TICKS(250));
                } else { accelerating = false; }
            }
            if (sim_speed > 175) accelerating = false;
        } else {
            sim_speed -= 0.8; sim_rpm -= 60;
            if (sim_speed <= 0) {
                sim_speed = 0; sim_rpm = 800; gear = 1; accelerating = true;
                vTaskDelay(pdMS_TO_TICKS(1500)); 
            }
        }
        car_speed = (int)sim_speed;
        car_rpm = (int)sim_rpm;
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
#else
    // Real OBD Logic... (Omitted for brevity)
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif
}

static void obd_gesture_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
        stop_obd_wifi();
        switch_to_launcher();
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

    lv_meter_scale_t * scale = lv_meter_add_scale(rpm_meter);
    lv_meter_set_scale_range(rpm_meter, scale, 0, 8000, 270, 135);
    
    // 1. WHITE TICKS
    lv_meter_set_scale_ticks(rpm_meter, scale, 41, 2, 12, lv_color_hex(0xFFFFFF)); 
    lv_meter_set_scale_major_ticks(rpm_meter, scale, 5, 4, 22, lv_color_hex(0xFFFFFF), 100); 

    // 2. Manual labels 1-8 - MOVED OUTWARD to r=180 to clear Speedometer
    for(int i=1; i<=8; i++) {
        lv_obj_t * lbl = lv_label_create(obd_screen);
        lv_label_set_text_fmt(lbl, "%d", i);
        float angle_deg = 135.0f + (i * 33.75f); 
        float angle_rad = angle_deg * M_PI / 180.0f;
        int r = 180; // Optimized radius to tuck labels next to ticks
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(lbl, (i >= 6) ? lv_color_hex(0xFF0000) : lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, (int)(r * cos(angle_rad)), (int)(r * sin(angle_rad)));
    }

    // 3. Redline styling
    lv_meter_indicator_t * red_arc = lv_meter_add_arc(rpm_meter, scale, 15, lv_color_hex(0xFF0000), 0);
    lv_meter_set_indicator_start_value(rpm_meter, red_arc, 6000);
    lv_meter_set_indicator_end_value(rpm_meter, red_arc, 8000);

    lv_meter_indicator_t * red_ticks = lv_meter_add_scale_lines(rpm_meter, scale, lv_color_hex(0xFF0000), lv_color_hex(0xFF0000), false, 0);
    lv_meter_set_indicator_start_value(rpm_meter, red_ticks, 6000);
    lv_meter_set_indicator_end_value(rpm_meter, red_ticks, 8000);

    // 4. Orange Active Arc
    rpm_indicator = lv_meter_add_arc(rpm_meter, scale, 15, lv_color_hex(0xE67E22), 0);
    lv_meter_set_indicator_end_value(rpm_meter, rpm_indicator, 0);

    // 5. Speed Label (MASSIVE 200px)
    speed_label = lv_label_create(obd_screen);
    lv_label_set_text(speed_label, "0");
    lv_obj_set_style_text_color(speed_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(speed_label, &ui_font_rajdhani200, 0);
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t * unit_lbl = lv_label_create(obd_screen);
    lv_label_set_text(unit_lbl, "km/h");
    lv_obj_set_style_text_color(unit_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(unit_lbl, &lv_font_montserrat_28, 0);
    lv_obj_align(unit_lbl, LV_ALIGN_CENTER, 0, 85);

    // 6. x1000 rpm Label (6 O'Clock)
    lv_obj_t * rpm_unit_lbl = lv_label_create(obd_screen);
    lv_label_set_text(rpm_unit_lbl, "x 1000 rpm");
    lv_obj_set_style_text_color(rpm_unit_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(rpm_unit_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(rpm_unit_lbl, LV_ALIGN_BOTTOM_MID, 0, -40);

    // Universal Exit
    lv_obj_t * touch_overlay = lv_obj_create(obd_screen);
    lv_obj_set_size(touch_overlay, 466, 466);
    lv_obj_set_style_bg_opa(touch_overlay, 0, 0);
    lv_obj_set_style_border_width(touch_overlay, 0, 0);
    lv_obj_add_flag(touch_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(touch_overlay, obd_gesture_cb, LV_EVENT_LONG_PRESSED, NULL);
}

void switch_to_obd() {
    current_state = STATE_OBD;
    lv_scr_load_anim(obd_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
    start_obd_wifi();
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
    static int last_s = -1, last_r = -1;
    static uint32_t last_ui_update = 0;

    if (millis() - last_ui_update < 100) return;
    last_ui_update = millis();

    bool changed = false;
    if (car_rpm != last_r) {
        lv_meter_set_indicator_end_value(rpm_meter, rpm_indicator, car_rpm);
        last_r = car_rpm;
        changed = true;
    }
    if (car_speed != last_s) {
        lv_label_set_text_fmt(speed_label, "%d", car_speed);
        last_s = car_speed;
        changed = true;
    }

    if (changed) {
        lv_obj_invalidate(obd_screen);
    }
}

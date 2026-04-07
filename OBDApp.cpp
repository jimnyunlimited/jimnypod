#include "OBDApp.h"
#include "jimnypod.h"
#include "lcd_bsp.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <math.h>

// --- SIMULATION TOGGLE ---
#define SIMULATE_OBD 1

// Declare the large custom font
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

// --- Background Worker ---
void read_obd_response(WiFiClient& client, char* buffer, size_t max_len) {
    unsigned long timeout = millis() + 1500; 
    int len = 0;
    buffer[0] = '\0';
    while (millis() < timeout) {
        if (client.available()) {
            char c = client.read();
            if (c == '\r' || c == '\n') continue; 
            if (c == '>') { buffer[len] = '\0'; break; }
            if (len < max_len - 1) buffer[len++] = c;
        }
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
    buffer[len] = '\0';
}

void obdBackgroundWorker(void *pvParameters) {
#if SIMULATE_OBD
    float sim_speed = 0;
    float sim_rpm = 800;
    bool speed_up = true;
    while(1) {
        if (speed_up) {
            sim_speed += 0.3; sim_rpm += 15;
            if (sim_speed > 140) speed_up = false;
        } else {
            sim_speed -= 0.5; sim_rpm -= 25;
            if (sim_speed < 0) speed_up = true;
        }
        car_speed = (int)sim_speed;
        car_rpm = (int)sim_rpm;
        if (car_rpm > 8000) car_rpm = 8000; // Cap for stability
        vTaskDelay(pdMS_TO_TICKS(150)); 
    }
#else
    WiFiClient client;
    char rx_buf[64];
    while(1) {
        if (!obd_wifi_running) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.begin(obd_ssid);
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 10) { vTaskDelay(pdMS_TO_TICKS(500)); attempts++; }
            if (WiFi.status() != WL_CONNECTED) { vTaskDelay(pdMS_TO_TICKS(2000)); continue; }
        }
        if (!client.connected()) {
            if (client.connect(obd_ip, obd_port)) {
                client.print("ATZ\r"); read_obd_response(client, rx_buf, sizeof(rx_buf));
                client.print("ATE0\r"); read_obd_response(client, rx_buf, sizeof(rx_buf));
            } else { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
        }
        client.print("010C\r"); read_obd_response(client, rx_buf, sizeof(rx_buf));
        char* ptr = strstr(rx_buf, "41 0C");
        if (ptr) { int a, b; if (sscanf(ptr, "41 0C %x %x", &a, &b) == 2) car_rpm = ((a * 256) + b) / 4; }
        if (car_rpm > 8000) car_rpm = 8000;
        client.print("010D\r"); read_obd_response(client, rx_buf, sizeof(rx_buf));
        ptr = strstr(rx_buf, "41 0D");
        if (ptr) { int a; if (sscanf(ptr, "41 0D %x", &a) == 1) car_speed = a; }
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
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
    lv_obj_set_style_bg_opa(obd_screen, LV_OPA_COVER, 0); // Solid black bg
    lv_obj_clear_flag(obd_screen, LV_OBJ_FLAG_SCROLLABLE);

    rpm_meter = lv_meter_create(obd_screen);
    lv_obj_set_size(rpm_meter, 460, 460);
    lv_obj_center(rpm_meter);
    lv_obj_set_style_bg_opa(rpm_meter, 0, 0);
    lv_obj_set_style_border_width(rpm_meter, 0, 0);

    lv_meter_scale_t * scale = lv_meter_add_scale(rpm_meter);
    lv_meter_set_scale_range(rpm_meter, scale, 0, 8000, 270, 135);
    lv_meter_set_scale_ticks(rpm_meter, scale, 41, 2, 15, lv_color_hex(0x444444)); 

    // Precise manual labels 1-8
    for(int i=1; i<=8; i++) {
        lv_obj_t * lbl = lv_label_create(obd_screen);
        lv_label_set_text_fmt(lbl, "%d", i);
        // Corrected Trig: 135 start + (i * 270/8)
        float angle_deg = 135.0f + (i * 33.75f); 
        float angle_rad = angle_deg * M_PI / 180.0f;
        int r = 195; // Radius
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(lbl, (i >= 6) ? lv_color_hex(0xFF0000) : lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, (int)(r * cos(angle_rad)), (int)(r * sin(angle_rad)));
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
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * unit_lbl = lv_label_create(obd_screen);
    lv_label_set_text(unit_lbl, "km/h");
    lv_obj_set_style_text_color(unit_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(unit_lbl, &lv_font_montserrat_28, 0);
    lv_obj_align(unit_lbl, LV_ALIGN_CENTER, 0, 105);

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
    obd_wifi_running = true;
    return;
#endif
    if (obd_wifi_running) return;
    WiFi.mode(WIFI_STA);
    WiFi.begin(obd_ssid);
    obd_wifi_running = true;
    Serial.println("✓ OBD WiFi Started");
}

void stop_obd_wifi() {
    if (!obd_wifi_running) return;
#if !SIMULATE_OBD
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
#endif
    obd_wifi_running = false;
    Serial.println("✓ OBD WiFi Stopped");
}

void obd_loop_handler() {
    if (current_state != STATE_OBD) return;
    static int last_s = -1, last_r = -1;
    static uint32_t last_ui_update = 0;

    // CAP UI UPDATE TO 400ms (2.5Hz) TO PREVENT GARBLING OF HUGE FONT
    if (millis() - last_ui_update < 400) return;
    last_ui_update = millis();

    if (car_rpm != last_r) {
        lv_meter_set_indicator_end_value(rpm_meter, rpm_indicator, car_rpm);
        last_r = car_rpm;
    }
    if (car_speed != last_s) {
        lv_label_set_text_fmt(speed_label, "%d", car_speed);
        last_s = car_speed;
    }
}

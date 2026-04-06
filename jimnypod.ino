/**
 * Trailmaster OS - App Launcher, Inclinometer, & Photo Frame
 * Waveshare ESP32-S3-Touch-AMOLED-1.43
 */

#include <Arduino.h>
#include <FFat.h>
#include "lcd_bsp.h"
#include "FT3168.h"
#include "i2c_bsp.h"
#include "lvgl.h"
#include "jimnypod.h"
#include "InclinometerApp.h"
#include "PhotoFrameApp.h"
#include <math.h>
#include <vector>

// --- Global State Machine ---
AppState current_state = STATE_LAUNCHER;

lv_obj_t * launcher_screen = NULL;
lv_obj_t * settings_screen = NULL;
lv_obj_t * brightness_overlay = NULL;

// --- Settings Globals ---
int current_brightness = 100; // 10 to 100
lv_obj_t * brightness_label = NULL;

// --- Function Prototypes ---
void build_launcher_screen();
void build_settings_screen();
void set_brightness(int value);
void setup_brightness_overlay();

// --- Callbacks ---
static void btn_brightness_minus_cb(lv_event_t * e) {
    set_brightness(current_brightness - 10);
}

static void btn_brightness_plus_cb(lv_event_t * e) {
    set_brightness(current_brightness + 10);
}

static void settings_gesture_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_LONG_PRESSED) {
        switch_to_launcher();
    }
}

// --- Screen Builders ---
// ==========================================
//             APP LAUNCHER
// ==========================================
static void app_btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        int app_id = (int)(intptr_t)lv_event_get_user_data(e);
        if (app_id == 1) {
            switch_to_inclinometer();
        } else if (app_id == 2) {
            switch_to_photoframe();
        } else if (app_id == 3) {
            switch_to_settings();
        }
    }
}

lv_obj_t * create_launcher_btn(lv_obj_t * parent, const char * icon_str, const char * text, int app_id, uint32_t color_hex) {
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 300, 90);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 45, 0); // Pill shape
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(btn, app_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)app_id);

    // App Icon
    lv_obj_t * icon = lv_label_create(btn);
    lv_label_set_text(icon, icon_str);
    lv_obj_set_style_text_color(icon, lv_color_hex(color_hex), 0);
    #if LV_FONT_MONTSERRAT_32
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_32, 0);
    #else
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    #endif
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 30, 0);

    // App Name
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    #if LV_FONT_MONTSERRAT_24
        lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    #else
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    #endif
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 80, 0);
    
    return btn;
}

void build_launcher_screen() {
    launcher_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(launcher_screen, lv_color_hex(0x000000), 0); // True AMOLED Black
    
    // Enable scrolling and hide the scrollbar for a clean look
    lv_obj_add_flag(launcher_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(launcher_screen, LV_SCROLLBAR_MODE_OFF);

    // Set up a vertical flex layout
    lv_obj_set_flex_flow(launcher_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(launcher_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Add padding so the first and last items can be scrolled to the exact center of the round screen
    // Screen height is 466. Center is 233. Button height is 90 (center is 45). 233 - 45 = 188px padding.
    lv_obj_set_style_pad_top(launcher_screen, 188, 0);
    lv_obj_set_style_pad_bottom(launcher_screen, 188, 0);
    lv_obj_set_style_pad_row(launcher_screen, 20, 0); // 20px gap between buttons

    // --- Add Apps to Launcher ---
    create_launcher_btn(launcher_screen, LV_SYMBOL_GPS, "Inclinometer", 1, 0xE67E22);
    create_launcher_btn(launcher_screen, LV_SYMBOL_IMAGE, "Photo Frame", 2, 0x2ECC71);
    create_launcher_btn(launcher_screen, LV_SYMBOL_SETTINGS, "Settings", 3, 0x9E9E9E);
    
    // To add more apps in the future, just add another line here!
    // create_launcher_btn(launcher_screen, LV_SYMBOL_AUDIO, "Music", 4, 0x3498DB);
}

// --- Screen Switchers ---
void switch_to_launcher() {
    current_state = STATE_LAUNCHER;
    lv_scr_load_anim(launcher_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
}

void build_settings_screen() {
    settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_add_event_cb(settings_screen, settings_gesture_cb, LV_EVENT_LONG_PRESSED, NULL);

    lv_obj_t * title = lv_label_create(settings_screen);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    #if LV_FONT_MONTSERRAT_24
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    #endif
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    lv_obj_t * brightness_title = lv_label_create(settings_screen);
    lv_label_set_text(brightness_title, "Brightness");
    lv_obj_set_style_text_color(brightness_title, lv_color_hex(0xAAAAAA), 0);
    #if LV_FONT_MONTSERRAT_20
        lv_obj_set_style_text_font(brightness_title, &lv_font_montserrat_20, 0);
    #endif
    lv_obj_align(brightness_title, LV_ALIGN_CENTER, 0, -60);

    // Minus button
    lv_obj_t * btn_minus = lv_btn_create(settings_screen);
    lv_obj_set_size(btn_minus, 70, 70);
    lv_obj_align(btn_minus, LV_ALIGN_CENTER, -90, 10);
    lv_obj_set_style_bg_color(btn_minus, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(btn_minus, 35, 0);
    lv_obj_add_event_cb(btn_minus, btn_brightness_minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * label_minus = lv_label_create(btn_minus);
    lv_label_set_text(label_minus, "-");
    #if LV_FONT_MONTSERRAT_28
        lv_obj_set_style_text_font(label_minus, &lv_font_montserrat_28, 0);
    #endif
    lv_obj_center(label_minus);

    // Plus button
    lv_obj_t * btn_plus = lv_btn_create(settings_screen);
    lv_obj_set_size(btn_plus, 70, 70);
    lv_obj_align(btn_plus, LV_ALIGN_CENTER, 90, 10);
    lv_obj_set_style_bg_color(btn_plus, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(btn_plus, 35, 0);
    lv_obj_add_event_cb(btn_plus, btn_brightness_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * label_plus = lv_label_create(btn_plus);
    lv_label_set_text(label_plus, "+");
    #if LV_FONT_MONTSERRAT_28
        lv_obj_set_style_text_font(label_plus, &lv_font_montserrat_28, 0);
    #endif
    lv_obj_center(label_plus);

    // Brightness value label
    brightness_label = lv_label_create(settings_screen);
    lv_label_set_text_fmt(brightness_label, "%d%%", current_brightness);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xFFFFFF), 0);
    #if LV_FONT_MONTSERRAT_28
        lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_28, 0);
    #endif
    lv_obj_align(brightness_label, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t * hint = lv_label_create(settings_screen);
    lv_label_set_text(hint, "Long-press to exit");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -40);
}

void switch_to_settings() {
    current_state = STATE_SETTINGS;
    lv_scr_load_anim(settings_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
}

void set_brightness(int value) {
    if (value < 10) value = 10;
    if (value > 100) value = 100;
    current_brightness = value;
    
    if (brightness_label != NULL) {
        lv_label_set_text_fmt(brightness_label, "%d%%", current_brightness);
    }
    
    if (brightness_overlay != NULL) {
        int opa = 255 - (current_brightness * 255 / 100);
        lv_obj_set_style_bg_opa(brightness_overlay, opa, 0);
    }
}

void setup_brightness_overlay() {
    if (brightness_overlay == NULL) {
        brightness_overlay = lv_obj_create(lv_layer_sys());
        lv_obj_set_size(brightness_overlay, 466, 466);
        lv_obj_align(brightness_overlay, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(brightness_overlay, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(brightness_overlay, 0, 0); 
        lv_obj_set_style_border_width(brightness_overlay, 0, 0);
        lv_obj_clear_flag(brightness_overlay, LV_OBJ_FLAG_CLICKABLE); 
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║ TRAILMASTER OS - Multi App Environment  ║");
    Serial.println("╚════════════════════════════════════════╝\n");

    if (ESP.getPsramSize() == 0) {
        Serial.println("⚠ FATAL ERROR: PSRAM IS NOT DETECTED!");
        while(1) { delay(1000); }
    }

    I2C_master_Init(); 
    Touch_Init();
    lcd_lvgl_Init();

    // Init IMU
    inclinometer_setup_imu();

    // Photo Frame Setup (Includes PSRAM allocation and WiFi)
    photoframe_setup();

    // Mount FFat
    if (FFat.begin(true)) {
        Serial.printf("✓ FFat Mounted! Total space: %u KB\n", FFat.totalBytes() / 1024);
    }

    // Build Screens
    build_launcher_screen();
    build_inclinometer_screen();
    build_photoframe_screen();
    build_settings_screen();
    setup_brightness_overlay();

    // Start on Launcher
    switch_to_launcher();
}

void loop() {
    // Handle App Specific Logic
    inclinometer_loop_handler();
    photoframe_loop_handler();

    lv_timer_handler();
    delay(5);
}

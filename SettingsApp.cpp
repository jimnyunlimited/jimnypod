#include "SettingsApp.h"
#include "jimnypod.h"

// --- Settings Globals ---
int current_brightness = 100; // 10 to 100
lv_obj_t * settings_screen = NULL;
lv_obj_t * brightness_label = NULL;
lv_obj_t * brightness_overlay = NULL;

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

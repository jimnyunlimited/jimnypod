/**
 * Trailmaster OS - App Launcher, Inclinometer, & Photo Frame
 * Waveshare ESP32-S3-Touch-AMOLED-1.43
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <FFat.h>
#include "lcd_bsp.h"
#include "FT3168.h"
#include "i2c_bsp.h"
#include "lvgl.h"
#include "qmi8658c.h"
#include <math.h>
#include <vector>

// --- Global State Machine ---
enum AppState { STATE_LAUNCHER, STATE_INCLINOMETER, STATE_PHOTOFRAME, STATE_SETTINGS };
AppState current_state = STATE_LAUNCHER;

lv_obj_t * launcher_screen = NULL;
lv_obj_t * inclinometer_screen = NULL;
lv_obj_t * photoframe_screen = NULL;
lv_obj_t * settings_screen = NULL;
lv_obj_t * brightness_overlay = NULL;

// --- Settings Globals ---
int current_brightness = 100; // 10 to 100
lv_obj_t * brightness_label = NULL;

// --- Function Prototypes ---
void build_launcher_screen();
void build_inclinometer_screen();
void build_photoframe_screen();
void build_settings_screen();
void switch_to_launcher();
void switch_to_settings();
void set_brightness(int value);
void setup_brightness_overlay();

// --- Photo Frame Globals ---
const char* AP_SSID = "Trailmaster_Sync"; 
const char* AP_PASSWORD = "password123";
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
File uploadFile;
std::vector<String> image_files;
int current_image_index = 0;
volatile bool new_image_uploaded = false;
unsigned long last_image_change = 0;
const unsigned long SLIDESHOW_INTERVAL = 5000;
bool delete_dialog_open = false;
LV_IMG_DECLARE(qrcode); // Make sure your qrcode.c file is in the sketch folder!
lv_obj_t * img_obj;
lv_obj_t * wifi_screen_cont;
uint8_t * psram_buffer = NULL;
lv_img_dsc_t img_dsc;

// --- Inclinometer Globals ---
lv_obj_t * ground_line;
lv_point_t ground_points[2];
#define LADDER_COUNT 13
lv_obj_t * ladder_lines[LADDER_COUNT];
lv_point_t ladder_points[LADDER_COUNT][2];
lv_obj_t * ladder_labels_l[LADDER_COUNT];
lv_obj_t * ladder_labels_r[LADDER_COUNT];
#define ROLL_MARKER_COUNT 19
lv_obj_t * roll_markers[ROLL_MARKER_COUNT];
lv_point_t roll_marker_points[ROLL_MARKER_COUNT][3];
lv_obj_t * roll_pointer_line;
lv_point_t roll_pointer_points[3];
struct LadderLine { float x1, x2, y; int degree; };
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
#define ALPHA 0.85 
unsigned long last_time = 0;
bool imu_ready = false;

// --- HTML Frontend ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Trailmaster Sync</title>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=0">
    <style>
        :root { --primary: #e67e22; --bg: #121212; --card: #1e1e1e; --text: #f5f5f5; }
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 0; display: flex; flex-direction: column; align-items: center; overscroll-behavior-y: contain; }
        .header { background: #000; width: 100%; padding: 20px 0; text-align: center; border-bottom: 3px solid var(--primary); }
        .header h1 { margin: 0; font-size: 24px; letter-spacing: 2px; text-transform: uppercase; display: flex; align-items: center; justify-content: center; gap: 10px; }
        .container { padding: 20px; width: 100%; max-width: 400px; box-sizing: border-box; }
        .card { background: var(--card); padding: 25px; border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.5); text-align: center; }
        .warning-banner { background: rgba(230, 126, 34, 0.1); border: 1px solid var(--primary); color: #e67e22; padding: 10px; border-radius: 8px; font-size: 13px; margin-bottom: 20px; text-align: left; line-height: 1.4; }
        .upload-btn-wrapper { position: relative; overflow: hidden; display: inline-block; width: 100%; margin-bottom: 15px; }
        .btn-outline { border: 2px dashed #555; color: #aaa; background: transparent; padding: 20px; border-radius: 8px; width: 100%; font-size: 16px; cursor: pointer; transition: 0.3s; }
        .btn-outline:hover { border-color: var(--primary); color: var(--primary); }
        .upload-btn-wrapper input[type=file] { font-size: 100px; position: absolute; left: 0; top: 0; opacity: 0; cursor: pointer; height: 100%; }
        .btn-primary { background: var(--primary); color: white; border: none; padding: 15px; font-size: 16px; border-radius: 8px; cursor: pointer; width: 100%; font-weight: bold; text-transform: uppercase; transition: 0.3s; margin-top: 15px; }
        .btn-primary:hover { background: #d35400; }
        .btn-primary:disabled { background: #555; color: #888; cursor: not-allowed; }
        #editorControls { display: none; width: 100%; }
        .preview-container { margin: 0 auto 15px auto; width: 240px; height: 240px; border-radius: 50%; overflow: hidden; border: 4px solid #333; box-shadow: 0 0 20px rgba(0,0,0,0.8); position: relative; background: #000; touch-action: none; }
        canvas { width: 100%; height: 100%; cursor: grab; }
        canvas:active { cursor: grabbing; }
        .slider-container { display: flex; align-items: center; gap: 10px; margin-bottom: 15px; }
        .slider-container input[type=range] { flex-grow: 1; accent-color: var(--primary); }
        .hint { font-size: 12px; color: #888; margin-bottom: 15px; }
        .settings { margin-top: 20px; text-align: left; background: #252525; padding: 15px; border-radius: 8px; font-size: 14px; }
        .settings summary { cursor: pointer; font-weight: bold; color: #aaa; outline: none; }
        .settings label { display: block; margin-top: 10px; cursor: pointer; color: #ccc; }
        #status { margin-top: 15px; font-weight: bold; min-height: 20px; color: var(--primary); }
    </style>
</head>
<body>
    <div class="header">
        <h1>
            <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="var(--primary)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0 1 18 0z"></path>
                <circle cx="12" cy="10" r="3"></circle>
            </svg>
            TRAILMASTER
        </h1>
    </div>
    <div class="container">
        <div class="card">
            <div class="warning-banner" id="macWarning">
                <strong>Notice:</strong> If tapping the button below does nothing, please close this popup window, open <b>Safari</b> or <b>Chrome</b>, and go to <b>http://trailmaster.io</b>
            </div>
            <p style="color: #aaa; margin-top: 0; margin-bottom: 20px;">Sync device backgrounds</p>
            <div class="upload-btn-wrapper">
                <button class="btn-outline" id="selectBtn">Tap to Select Photo</button>
                <input type="file" id="fileInput" accept="image/*" onchange="loadImage()">
            </div>
            <div id="editorControls">
                <div class="preview-container" id="previewContainer">
                    <canvas id="canvas" width="466" height="466"></canvas>
                </div>
                <div class="hint">Drag image to pan</div>
                <div class="slider-container">
                    <span style="font-size: 18px;">-</span>
                    <input type="range" id="zoomSlider" min="1" max="3" step="0.01" value="1">
                    <span style="font-size: 18px;">+</span>
                </div>
            </div>
            <button class="btn-primary" id="uploadBtn" onclick="upload()" disabled>Send to Device</button>
            <div id="status"></div>
            <details class="settings">
                <summary>Advanced Settings</summary>
                <label><input type="checkbox" id="swapBytes" checked> Byte Swap (Fixes Static)</label>
                <label><input type="checkbox" id="swapRB"> Red/Blue Swap (Fixes Colors)</label>
            </details>
        </div>
    </div>
    <script>
        let img = new Image();
        let canvas = document.getElementById('canvas');
        let ctx = canvas.getContext('2d');
        let zoomSlider = document.getElementById('zoomSlider');
        let scale = 1, minScale = 1;
        let offsetX = 0, offsetY = 0;
        let isDragging = false;
        let startX, startY;
        let imageReady = false;

        async function loadImage() {
            const file = document.getElementById('fileInput').files[0];
            if (!file) return;
            document.getElementById('selectBtn').innerText = "Change Photo";
            img.src = URL.createObjectURL(file);
            await new Promise(r => img.onload = r);
            minScale = Math.max(466 / img.width, 466 / img.height);
            scale = minScale;
            offsetX = (466 - img.width * scale) / 2;
            offsetY = (466 - img.height * scale) / 2;
            zoomSlider.min = minScale;
            zoomSlider.max = minScale * 3;
            zoomSlider.value = scale;
            document.getElementById('editorControls').style.display = 'block';
            document.getElementById('uploadBtn').disabled = false;
            imageReady = true;
            draw();
        }

        function draw() {
            if (!imageReady) return;
            const maxOffsetX = 0;
            const minOffsetX = 466 - (img.width * scale);
            const maxOffsetY = 0;
            const minOffsetY = 466 - (img.height * scale);
            if (offsetX > maxOffsetX) offsetX = maxOffsetX;
            if (offsetX < minOffsetX) offsetX = minOffsetX;
            if (offsetY > maxOffsetY) offsetY = maxOffsetY;
            if (offsetY < minOffsetY) offsetY = minOffsetY;
            ctx.fillStyle = 'black';
            ctx.fillRect(0, 0, 466, 466);
            ctx.drawImage(img, offsetX, offsetY, img.width * scale, img.height * scale);
        }

        function getPos(e) {
            if(e.touches) return {x: e.touches[0].clientX, y: e.touches[0].clientY};
            return {x: e.clientX, y: e.clientY};
        }

        canvas.onmousedown = canvas.ontouchstart = (e) => {
            e.preventDefault();
            isDragging = true;
            let pos = getPos(e);
            startX = pos.x;
            startY = pos.y;
        };

        window.onmousemove = window.ontouchmove = (e) => {
            if (!isDragging) return;
            e.preventDefault();
            let pos = getPos(e);
            let rect = canvas.getBoundingClientRect();
            let scaleFactor = 466 / rect.width;
            offsetX += (pos.x - startX) * scaleFactor;
            offsetY += (pos.y - startY) * scaleFactor;
            startX = pos.x;
            startY = pos.y;
            draw();
        };

        window.onmouseup = window.ontouchend = () => { isDragging = false; };

        zoomSlider.oninput = (e) => {
            let oldScale = scale;
            scale = parseFloat(e.target.value);
            let centerX = 466 / 2;
            let centerY = 466 / 2;
            offsetX = centerX - (centerX - offsetX) * (scale / oldScale);
            offsetY = centerY - (centerY - offsetY) * (scale / oldScale);
            draw();
        };

        async function upload() {
            if (!imageReady) return;
            document.getElementById('status').innerText = 'Processing image...';
            document.getElementById('uploadBtn').disabled = true;
            const swapBytes = document.getElementById('swapBytes').checked;
            const swapRB = document.getElementById('swapRB').checked;
            const imgData = ctx.getImageData(0, 0, 466, 466).data;
            const rgb565Data = new Uint8Array(466 * 466 * 2);
            let j = 0;
            for (let i = 0; i < imgData.length; i += 4) {
                let r = imgData[i];
                let g = imgData[i+1];
                let b = imgData[i+2];
                if (swapRB) { let temp = r; r = b; b = temp; }
                const rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                if (swapBytes) {
                    rgb565Data[j++] = (rgb565 >> 8) & 0xFF;
                    rgb565Data[j++] = rgb565 & 0xFF;
                } else {
                    rgb565Data[j++] = rgb565 & 0xFF;
                    rgb565Data[j++] = (rgb565 >> 8) & 0xFF;
                }
            }
            document.getElementById('status').innerText = 'Uploading to Device...';
            const blob = new Blob([rgb565Data], {type: 'application/octet-stream'});
            const filename = "img_" + Date.now() + ".bin";
            const formData = new FormData();
            formData.append('image', blob, filename);
            try {
                const res = await fetch('/upload', { method: 'POST', body: formData });
                if (res.ok) {
                    document.getElementById('status').innerText = 'Sync Complete!';
                    document.getElementById('status').style.color = '#4CAF50';
                } else {
                    document.getElementById('status').innerText = 'Upload Failed.';
                    document.getElementById('status').style.color = '#f44336';
                }
            } catch (e) {
                document.getElementById('status').innerText = 'Error: ' + e;
                document.getElementById('status').style.color = '#f44336';
            }
            document.getElementById('uploadBtn').disabled = false;
        }
    </script>
</body>
</html>
)rawliteral";

// --- Function Prototypes ---
void scan_images();
void show_image(int index);
void next_image();
void prev_image();
void delete_current_image();
void updateAttitude();
void updateUI();
void switch_to_launcher();
void switch_to_inclinometer();
void switch_to_photoframe();

// --- Callbacks ---
static void btn_yes_cb(lv_event_t * e) {
    lv_obj_t * overlay = (lv_obj_t *)lv_event_get_user_data(e);
    delete_current_image();
    lv_obj_del(overlay);
    delete_dialog_open = false;
    last_image_change = millis(); 
}

static void btn_no_cb(lv_event_t * e) {
    lv_obj_t * overlay = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_del(overlay);
    delete_dialog_open = false;
    last_image_change = millis(); 
}

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

static void photoframe_gesture_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_LEFT) {
            next_image();
        } else if (dir == LV_DIR_RIGHT) {
            prev_image();
        } else if (dir == LV_DIR_TOP) { 
            // SWIPE UP TO DELETE
            if (!image_files.empty() && !delete_dialog_open) {
                delete_dialog_open = true; 
                lv_obj_t * overlay = lv_obj_create(photoframe_screen);
                lv_obj_set_size(overlay, 466, 466);
                lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
                lv_obj_set_style_bg_opa(overlay, 200, 0);
                lv_obj_set_style_border_width(overlay, 0, 0);
                lv_obj_center(overlay);
                
                lv_obj_t * label = lv_label_create(overlay);
                lv_label_set_text(label, "Delete this photo?");
                lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
                #if LV_FONT_MONTSERRAT_24
                    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
                #endif
                lv_obj_align(label, LV_ALIGN_CENTER, 0, -40);
                
                lv_obj_t * btn_yes = lv_btn_create(overlay);
                lv_obj_set_size(btn_yes, 120, 50);
                lv_obj_align(btn_yes, LV_ALIGN_CENTER, -70, 30);
                lv_obj_set_style_bg_color(btn_yes, lv_color_hex(0xD32F2F), 0);
                lv_obj_add_event_cb(btn_yes, btn_yes_cb, LV_EVENT_CLICKED, overlay);
                lv_obj_t * label_yes = lv_label_create(btn_yes);
                lv_label_set_text(label_yes, "Yes");
                lv_obj_center(label_yes);
                
                lv_obj_t * btn_no = lv_btn_create(overlay);
                lv_obj_set_size(btn_no, 120, 50);
                lv_obj_align(btn_no, LV_ALIGN_CENTER, 70, 30);
                lv_obj_set_style_bg_color(btn_no, lv_color_hex(0x555555), 0);
                lv_obj_add_event_cb(btn_no, btn_no_cb, LV_EVENT_CLICKED, overlay);
                lv_obj_t * label_no = lv_label_create(btn_no);
                lv_label_set_text(label_no, "No");
                lv_obj_center(label_no);
            }
        }
    } 
    else if (code == LV_EVENT_LONG_PRESSED) {
        // UNIVERSAL EXIT: Long press to go back to launcher
        if (!delete_dialog_open) {
            switch_to_launcher();
        }
    }
}

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

void build_launcher_screen() {
    launcher_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(launcher_screen, lv_color_hex(0x000000), 0); // True AMOLED Black
    lv_obj_clear_flag(launcher_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t * title = lv_label_create(launcher_screen);
    lv_label_set_text(title, "TRAILMASTER");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    #if LV_FONT_MONTSERRAT_24
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    #endif
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -140);

    // Subtitle
    lv_obj_t * subtitle = lv_label_create(launcher_screen);
    lv_label_set_text(subtitle, "Select App");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x888888), 0);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, -110);

    // --- App 1: Inclinometer ---
    lv_obj_t * btn1 = lv_btn_create(launcher_screen);
    lv_obj_set_size(btn1, 120, 120);
    lv_obj_align(btn1, LV_ALIGN_CENTER, -80, -10);
    lv_obj_set_style_bg_color(btn1, lv_color_hex(0x1A1A1A), 0); 
    lv_obj_set_style_bg_color(btn1, lv_color_hex(0x333333), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn1, 30, 0); 
    lv_obj_set_style_border_width(btn1, 2, 0);
    lv_obj_set_style_border_color(btn1, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(btn1, app_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    lv_obj_t * icon1 = lv_label_create(btn1);
    lv_label_set_text(icon1, LV_SYMBOL_GPS); 
    lv_obj_set_style_text_color(icon1, lv_color_hex(0xE67E22), 0); 
    #if LV_FONT_MONTSERRAT_28
        lv_obj_set_style_text_font(icon1, &lv_font_montserrat_28, 0);
    #else
        lv_obj_set_style_text_font(icon1, &lv_font_montserrat_24, 0);
    #endif
    lv_obj_align(icon1, LV_ALIGN_CENTER, 0, -15);

    lv_obj_t * label1 = lv_label_create(btn1);
    lv_label_set_text(label1, "Incline");
    lv_obj_set_style_text_color(label1, lv_color_hex(0xFFFFFF), 0);
    #if LV_FONT_MONTSERRAT_16
        lv_obj_set_style_text_font(label1, &lv_font_montserrat_16, 0);
    #else
        lv_obj_set_style_text_font(label1, &lv_font_montserrat_20, 0);
    #endif
    lv_obj_align(label1, LV_ALIGN_CENTER, 0, 30);

    // --- App 2: Photo Frame ---
    lv_obj_t * btn2 = lv_btn_create(launcher_screen);
    lv_obj_set_size(btn2, 120, 120);
    lv_obj_align(btn2, LV_ALIGN_CENTER, 80, -10);
    lv_obj_set_style_bg_color(btn2, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_color(btn2, lv_color_hex(0x333333), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn2, 30, 0);
    lv_obj_set_style_border_width(btn2, 2, 0);
    lv_obj_set_style_border_color(btn2, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(btn2, app_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)2);

    lv_obj_t * icon2 = lv_label_create(btn2);
    lv_label_set_text(icon2, LV_SYMBOL_IMAGE); 
    lv_obj_set_style_text_color(icon2, lv_color_hex(0x2ECC71), 0); 
    #if LV_FONT_MONTSERRAT_28
        lv_obj_set_style_text_font(icon2, &lv_font_montserrat_28, 0);
    #else
        lv_obj_set_style_text_font(icon2, &lv_font_montserrat_24, 0);
    #endif
    lv_obj_align(icon2, LV_ALIGN_CENTER, 0, -15);

    lv_obj_t * label2 = lv_label_create(btn2);
    lv_label_set_text(label2, "Photos");
    lv_obj_set_style_text_color(label2, lv_color_hex(0xFFFFFF), 0);
    #if LV_FONT_MONTSERRAT_16
        lv_obj_set_style_text_font(label2, &lv_font_montserrat_16, 0);
    #else
        lv_obj_set_style_text_font(label2, &lv_font_montserrat_20, 0);
    #endif
    lv_obj_align(label2, LV_ALIGN_CENTER, 0, 30);

    // --- App 3: Settings ---
    lv_obj_t * btn3 = lv_btn_create(launcher_screen);
    lv_obj_set_size(btn3, 120, 120);
    lv_obj_align(btn3, LV_ALIGN_CENTER, 0, 120);
    lv_obj_set_style_bg_color(btn3, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_color(btn3, lv_color_hex(0x333333), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn3, 30, 0);
    lv_obj_set_style_border_width(btn3, 2, 0);
    lv_obj_set_style_border_color(btn3, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(btn3, app_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)3);

    lv_obj_t * icon3 = lv_label_create(btn3);
    lv_label_set_text(icon3, LV_SYMBOL_SETTINGS); 
    lv_obj_set_style_text_color(icon3, lv_color_hex(0x9E9E9E), 0);
    #if LV_FONT_MONTSERRAT_28
        lv_obj_set_style_text_font(icon3, &lv_font_montserrat_28, 0);
    #else
        lv_obj_set_style_text_font(icon3, &lv_font_montserrat_24, 0);
    #endif
    lv_obj_align(icon3, LV_ALIGN_CENTER, 0, -15);

    lv_obj_t * label3 = lv_label_create(btn3);
    lv_label_set_text(label3, "Settings");
    lv_obj_set_style_text_color(label3, lv_color_hex(0xFFFFFF), 0);
    #if LV_FONT_MONTSERRAT_16
        lv_obj_set_style_text_font(label3, &lv_font_montserrat_16, 0);
    #else
        lv_obj_set_style_text_font(label3, &lv_font_montserrat_20, 0);
    #endif
    lv_obj_align(label3, LV_ALIGN_CENTER, 0, 30);
}

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
// Listen to ALL touch events so we can track presses, swipes, and clicks
    lv_obj_add_event_cb(touch_overlay, inclinometer_gesture_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(touch_overlay, inclinometer_gesture_cb, LV_EVENT_LONG_PRESSED, NULL);

    // Call updateUI once to set initial points so it's not blank
    updateUI();    
}

void build_photoframe_screen() {
    photoframe_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(photoframe_screen, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(photoframe_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_add_event_cb(photoframe_screen, photoframe_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(photoframe_screen, photoframe_gesture_cb, LV_EVENT_LONG_PRESSED, NULL);

    img_obj = lv_img_create(photoframe_screen);
    lv_obj_center(img_obj);
    lv_obj_add_flag(img_obj, LV_OBJ_FLAG_HIDDEN);

    wifi_screen_cont = lv_obj_create(photoframe_screen);
    lv_obj_set_size(wifi_screen_cont, 466, 466);
    lv_obj_set_style_bg_color(wifi_screen_cont, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(wifi_screen_cont, 0, 0);
    lv_obj_clear_flag(wifi_screen_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(wifi_screen_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(wifi_screen_cont);

    lv_obj_t * title = lv_label_create(wifi_screen_cont);
    lv_label_set_text(title, "TRAILMASTER");
    lv_obj_set_style_text_color(title, lv_color_hex(0xe67e22), 0);
    #if LV_FONT_MONTSERRAT_28
        lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    #elif LV_FONT_MONTSERRAT_24
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    #endif
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -160);

    lv_obj_t * hardcoded_qr = lv_img_create(wifi_screen_cont);
    lv_img_set_src(hardcoded_qr, &qrcode); 
    lv_img_set_zoom(hardcoded_qr, 160); 
    lv_obj_align(hardcoded_qr, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t * net_box = lv_obj_create(wifi_screen_cont);
    lv_obj_set_size(net_box, 360, 100); 
    lv_obj_set_style_bg_color(net_box, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_color(net_box, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(net_box, 2, 0);
    lv_obj_set_style_radius(net_box, 15, 0); 
    lv_obj_align(net_box, LV_ALIGN_CENTER, 0, 125); 
    lv_obj_clear_flag(net_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(net_box, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * cred_lbl = lv_label_create(net_box);
    lv_label_set_text_fmt(cred_lbl, "WiFi: %s\nPass: %s", AP_SSID, AP_PASSWORD);
    lv_obj_set_style_text_color(cred_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_align(cred_lbl, LV_TEXT_ALIGN_CENTER, 0);
    #if LV_FONT_MONTSERRAT_24
        lv_obj_set_style_text_font(cred_lbl, &lv_font_montserrat_24, 0);
    #endif
    lv_obj_center(cred_lbl);

    lv_obj_t * url_lbl = lv_label_create(wifi_screen_cont);
    lv_label_set_text(url_lbl, "http://trailmaster.io");
    lv_obj_set_style_text_color(url_lbl, lv_color_hex(0x4CAF50), 0);
    #if LV_FONT_MONTSERRAT_24
        lv_obj_set_style_text_font(url_lbl, &lv_font_montserrat_24, 0);
    #endif
    lv_obj_align(url_lbl, LV_ALIGN_CENTER, 0, 195);
}


// --- Screen Switchers ---
void switch_to_launcher() {
    current_state = STATE_LAUNCHER;
    lv_scr_load_anim(launcher_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
}

void switch_to_inclinometer() {
    current_state = STATE_INCLINOMETER;
    lv_scr_load_anim(inclinometer_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
}

void switch_to_photoframe() {
    current_state = STATE_PHOTOFRAME;
    lv_scr_load_anim(photoframe_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
    scan_images();
    if (image_files.size() > 0) {
        show_image(0);
    }
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

    // Allocate PSRAM
    psram_buffer = (uint8_t *)heap_caps_malloc(466 * 466 * 2, MALLOC_CAP_SPIRAM);
    img_dsc.header.always_zero = 0;
    img_dsc.header.w = 466;
    img_dsc.header.h = 466;
    img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    img_dsc.data_size = 466 * 466 * 2;
    img_dsc.data = psram_buffer;

    // Mount FFat
    if (FFat.begin(true)) {
        Serial.printf("✓ FFat Mounted! Total space: %u KB\n", FFat.totalBytes() / 1024);
    }

    // Start WiFi & Web Server (Runs in background always)
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });
    server.on("/upload", HTTP_POST, []() { server.send(200, "text/plain", "OK"); }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            uploadFile = FFat.open(filename, FILE_WRITE);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
        } else if (upload.status == UPLOAD_FILE_END) {
            if (uploadFile) uploadFile.close();
            new_image_uploaded = true;
        }
    });
    server.onNotFound([]() {
        server.sendHeader("Location", "http://trailmaster.io/", true);
        server.send(302, "text/plain", "");
    });
    server.begin();

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
    // Always handle WiFi and Web Server
    dnsServer.processNextRequest();
    server.handleClient();

    if (new_image_uploaded) {
        new_image_uploaded = false;
        if (current_state == STATE_PHOTOFRAME) {
            scan_images();
            if (image_files.size() > 0) {
                current_image_index = image_files.size() - 1;
                show_image(current_image_index);
            }
        }
    }

    // Handle App Specific Logic
    if (current_state == STATE_INCLINOMETER) {
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
    else if (current_state == STATE_PHOTOFRAME) {
        if (image_files.size() > 1 && !delete_dialog_open) {
            if (millis() - last_image_change >= SLIDESHOW_INTERVAL) {
                next_image();
            }
        }
    }

    lv_timer_handler();
    delay(5);
}

// --- Photo Frame Functions ---
void scan_images() {
    image_files.clear();
    File root = FFat.open("/");
    File file = root.openNextFile();
    while (file) {
        String filename = file.name();
        if (filename.endsWith(".bin")) {
            if (!filename.startsWith("/")) filename = "/" + filename;
            image_files.push_back(filename);
        }
        file = root.openNextFile();
    }
}

void show_image(int index) {
    last_image_change = millis(); 
    if (image_files.empty() || index < 0 || index >= image_files.size()) {
        lv_obj_clear_flag(wifi_screen_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    String path = image_files[index];
    File file = FFat.open(path, FILE_READ);
    if (!file) return;
    size_t bytes_read = file.read(psram_buffer, 466 * 466 * 2);
    file.close();
    if (bytes_read > 0) {
        lv_obj_add_flag(wifi_screen_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(img_obj, &img_dsc);
    }
}

void next_image() {
    if (image_files.size() <= 1) return;
    current_image_index++;
    if (current_image_index >= image_files.size()) current_image_index = 0;
    show_image(current_image_index);
}

void prev_image() {
    if (image_files.size() <= 1) return;
    current_image_index--;
    if (current_image_index < 0) current_image_index = image_files.size() - 1;
    show_image(current_image_index);
}

void delete_current_image() {
    if (image_files.empty()) return;
    String path = image_files[current_image_index];
    FFat.remove(path);
    scan_images();
    if (image_files.empty()) {
        lv_obj_add_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(wifi_screen_cont, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (current_image_index >= image_files.size()) current_image_index = 0;
        show_image(current_image_index);
    }
}

// --- Inclinometer Functions ---
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
#include "GameApp.h"
#include "jimnypod.h"
#include "lcd_bsp.h" 
#include <Preferences.h>

// --- Game Globals ---
lv_obj_t * game_screen = NULL;
lv_obj_t * rhino_canvas = NULL; 
lv_obj_t * cactus_obj = NULL;
lv_obj_t * score_label = NULL;
lv_obj_t * high_score_label = NULL;
lv_obj_t * ground_line_obj = NULL;
lv_obj_t * countdown_label = NULL;

static lv_color_t rhino_buffer[100 * 80];

bool is_gaming = false;
bool is_jumping = false;
bool is_game_over = false;
int countdown_val = 0;
unsigned long countdown_start_ms = 0;

int game_score = 0;
int high_score = 0;
const int GROUND_Y = 300; 
const int DINO_X_POS = 100; // Moved further right
int rhino_y = GROUND_Y - 81;
float rhino_vf = 0; 
int cactus_x = 1000; 

Preferences gamePrefs;

void render_rhino_on_canvas(bool hit) {
    lv_canvas_fill_bg(rhino_canvas, lv_color_hex(0x000000), LV_OPA_COVER);
    
    lv_draw_rect_dsc_t draw_dsc;
    lv_draw_rect_dsc_init(&draw_dsc);
    draw_dsc.bg_color = hit ? lv_color_hex(0xFF0000) : lv_color_hex(0x888888);
    draw_dsc.radius = 8;
    lv_canvas_draw_rect(rhino_canvas, 10, 25, 60, 40, &draw_dsc);
    
    draw_dsc.bg_color = hit ? lv_color_hex(0xFF0000) : lv_color_hex(0xAAAAAA);
    lv_canvas_draw_rect(rhino_canvas, 60, 15, 30, 25, &draw_dsc);

    draw_dsc.bg_color = lv_color_hex(0xEEEEEE);
    lv_canvas_draw_rect(rhino_canvas, 82, 5, 8, 15, &draw_dsc); 
    lv_canvas_draw_rect(rhino_canvas, 72, 10, 6, 10, &draw_dsc); 

    draw_dsc.bg_color = hit ? lv_color_hex(0xAA0000) : lv_color_hex(0x666666);
    lv_canvas_draw_rect(rhino_canvas, 15, 65, 15, 15, &draw_dsc);
    lv_canvas_draw_rect(rhino_canvas, 45, 65, 15, 15, &draw_dsc);
}

static void start_countdown() {
    countdown_val = 3;
    countdown_start_ms = millis();
    lv_obj_clear_flag(countdown_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(countdown_label, "3");
    // Hide cactus and reset rhino
    cactus_x = 1000;
    lv_obj_set_x(cactus_obj, cactus_x);
}

static void game_gesture_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (is_game_over) {
            is_game_over = false;
            game_score = 0;
            rhino_y = GROUND_Y - 81;
            rhino_vf = 0;
            lv_label_set_text(score_label, "Score: 0");
            render_rhino_on_canvas(false);
            lv_obj_invalidate(game_screen); 
            start_countdown();
        } else if (!is_jumping && countdown_val == 0) {
            rhino_vf = -20.0f; 
            is_jumping = true;
        }
    } else if (code == LV_EVENT_LONG_PRESSED) {
        lv_obj_add_flag(score_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(high_score_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(countdown_label, LV_OBJ_FLAG_HIDDEN);
        switch_to_launcher();
    }
}

void build_game_screen() {
    game_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(game_screen, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(game_screen, LV_OBJ_FLAG_SCROLLABLE);

    cactus_obj = lv_obj_create(game_screen);
    lv_obj_set_size(cactus_obj, 25, 60);
    lv_obj_set_pos(cactus_obj, cactus_x, GROUND_Y - 60);
    lv_obj_set_style_bg_color(cactus_obj, lv_color_hex(0x07E000), 0);
    lv_obj_set_style_border_width(cactus_obj, 0, 0);
    lv_obj_set_style_pad_all(cactus_obj, 0, 0); // Remove padding
    lv_obj_set_style_radius(cactus_obj, 4, 0);

    rhino_canvas = lv_canvas_create(game_screen);
    lv_canvas_set_buffer(rhino_canvas, rhino_buffer, 100, 80, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_style_pad_all(rhino_canvas, 0, 0); // Remove padding
    render_rhino_on_canvas(false);

    ground_line_obj = lv_obj_create(game_screen);
    lv_obj_set_size(ground_line_obj, 466, 4);
    lv_obj_set_pos(ground_line_obj, 0, GROUND_Y);
    lv_obj_set_style_bg_color(ground_line_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(ground_line_obj, 0, 0);

    high_score_label = lv_label_create(lv_layer_top());
    lv_label_set_text(high_score_label, "Best: 0");
    lv_obj_set_style_text_color(high_score_label, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_text_font(high_score_label, &lv_font_montserrat_24, 0);
    lv_obj_align(high_score_label, LV_ALIGN_BOTTOM_MID, 0, -75);
    lv_obj_add_flag(high_score_label, LV_OBJ_FLAG_HIDDEN);

    score_label = lv_label_create(lv_layer_top());
    lv_label_set_text(score_label, "Score: 0");
    lv_obj_set_style_text_color(score_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_24, 0);
    lv_obj_align(score_label, LV_ALIGN_BOTTOM_MID, 0, -45);
    lv_obj_add_flag(score_label, LV_OBJ_FLAG_HIDDEN);

    // Countdown Label
    countdown_label = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_color(countdown_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(countdown_label, &lv_font_montserrat_48, 0); // Large font
    lv_obj_center(countdown_label);
    lv_obj_add_flag(countdown_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(game_screen, game_gesture_cb, LV_EVENT_ALL, NULL);
}

void switch_to_game() {
    current_state = STATE_GAME;
    is_gaming = true;
    is_game_over = false;
    game_score = 0;
    rhino_y = GROUND_Y - 81;
    rhino_vf = 0;
    
    lv_obj_clear_flag(score_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(high_score_label, LV_OBJ_FLAG_HIDDEN);

    gamePrefs.begin("game", true);
    high_score = gamePrefs.getInt("high_score", 0);
    gamePrefs.end();
    lv_label_set_text_fmt(high_score_label, "Best: %d", high_score);
    lv_label_set_text(score_label, "Score: 0");

    lv_scr_load_anim(game_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
    start_countdown();
}

void game_loop_handler() {
    if (current_state != STATE_GAME || !is_gaming) return;

    static uint32_t last_tick = 0;
    if (millis() - last_tick < 20) return;
    last_tick = millis();

    // Handle Countdown
    if (countdown_val > 0) {
        if (millis() - countdown_start_ms >= 1000) {
            countdown_val--;
            countdown_start_ms = millis();
            if (countdown_val > 0) {
                lv_label_set_text_fmt(countdown_label, "%d", countdown_val);
            } else {
                lv_obj_add_flag(countdown_label, LV_OBJ_FLAG_HIDDEN);
                cactus_x = 600; // Start the first cactus
            }
        }
        return; // Pause game logic during countdown
    }

    if (is_game_over) return;

    // Physics
    if (is_jumping) {
        rhino_vf += 1.0f; 
        rhino_y += (int)rhino_vf;
        if (rhino_y >= GROUND_Y - 81) {
            rhino_y = GROUND_Y - 81;
            is_jumping = false;
            rhino_vf = 0;
        }
    }
    lv_obj_set_y(rhino_canvas, rhino_y);
    lv_obj_invalidate(rhino_canvas);

    cactus_x -= (10 + (game_score / 5));
    if (cactus_x < -40) {
        cactus_x = 466;
        game_score++;
        lv_label_set_text_fmt(score_label, "Score: %d", game_score);
        if (game_score > high_score) {
            high_score = game_score;
            lv_label_set_text_fmt(high_score_label, "Best: %d", high_score);
        }
    }
    lv_obj_set_x(cactus_obj, cactus_x);

    // EVEN TIGHTER COLLISION
    // Rhino is at X=100. Body mass is visual X = 115 to 175.
    // Cactus is 25 wide.
    // Collision starts when cactus right edge (cactus_x + 25) hits rhino body left (115) -> cactus_x = 90
    // Collision ends when cactus left edge (cactus_x) leaves rhino head right (190) -> cactus_x = 190
    if (cactus_x > 90 && cactus_x < 185) {
        if (rhino_y + 72 > GROUND_Y - 60) {
            is_game_over = true;
            render_rhino_on_canvas(true); 
            gamePrefs.begin("game", false);
            gamePrefs.putInt("high_score", high_score);
            gamePrefs.end();
        }
    }
}

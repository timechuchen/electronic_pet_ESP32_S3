#include <string.h>
#include "pet_ui.h"
#include "pet_assets.h"
#include "ui_text.h"

LV_FONT_DECLARE(pet_font_zh_16);
LV_FONT_DECLARE(pet_font_zh_20);
LV_FONT_DECLARE(pet_font_zh_28);

enum {
    INK = 0xEEF5F4, MUTED = 0xA9BABB, ACCENT = 0x91E0CE,
    SURFACE = 0x172B32, OUTLINE = 0x43616A,
    AMBIENT_SCALE = 344, ACTIVE_SCALE = 256, TRANSITION_MS = 320
};

static pet_ui_callbacks_t cb;
static lv_obj_t *portrait, *status_text, *caption, *controls, *hint, *subtitle;
static lv_obj_t *btn[3], *btn_text[3], *dots[5], *spark[4];
static bool ambient = true, paused, was_live, transitioning;
static unsigned clip, frame, page, gesture;
static uint32_t last_interaction, last_gesture;
static int32_t presentation;
static const char *pages[5][3] = {
    {"wave", "dance", "more"}, {"smile", "wink", "more"},
    {"nod", "shake", "more"}, {"jump", "grace", "more"},
    {"blink", "pause", "home"}
};
static const char *page_labels[5][3] = UI_PAGE_LABELS;

static void visible(lv_obj_t *obj, bool show)
{
    if(show) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_labels(void)
{
    for(unsigned i = 0; i < 3; i++) {
        lv_label_set_text(btn_text[i], strcmp(pages[page][i], "pause") == 0 ?
                          (paused ? UI_CONTINUE : UI_PAUSE) : page_labels[page][i]);
    }
    for(unsigned i = 0; i < 5; i++) {
        lv_obj_set_style_bg_color(dots[i], lv_color_hex(i == page ? ACCENT : OUTLINE), 0);
        lv_obj_set_width(dots[i], i == page ? 14 : 5);
    }
    lv_label_set_text(hint, paused ? UI_PAUSED_HINT : UI_WAKE_HINT);
}

// One reversible animation owns the layout, even during rapid wake/rest taps.
static void set_presentation(void *unused, int32_t value)
{
    (void)unused;
    presentation = value;
    int32_t scale = AMBIENT_SCALE + (ACTIVE_SCALE - AMBIENT_SCALE) * value / 256;
    lv_image_set_scale(portrait, scale);
    lv_obj_set_pos(portrait, 73, 91 - 39 * value / 256);
    lv_obj_set_style_opa(controls, value * LV_OPA_COVER / 256, 0);
    lv_obj_set_style_opa(status_text, value * LV_OPA_COVER / 256, 0);
    lv_obj_set_style_opa(caption, value * LV_OPA_COVER / 256, 0);
}

static void transition_done(lv_anim_t *anim)
{
    (void)anim;
    transitioning = false;
}

static void set_ambient(bool value)
{
    last_interaction = lv_tick_get();
    if(ambient == value) return;
    ambient = value;
    // Reset pagination on rest; preserve an explicit pause until resumed.
    if(ambient) { page = 0; last_gesture = last_interaction; }
    refresh_labels();
    visible(controls, !ambient);
    visible(status_text, !ambient);
    visible(caption, !ambient);
    visible(hint, ambient);
    visible(subtitle, ambient);
    for(unsigned i = 0; i < 4; i++) visible(spark[i], ambient);
    lv_anim_delete(portrait, set_presentation);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, portrait);
    lv_anim_set_exec_cb(&anim, set_presentation);
    lv_anim_set_values(&anim, presentation, ambient ? 0 : 256);
    lv_anim_set_duration(&anim, TRANSITION_MS);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&anim, transition_done);
    transitioning = true;
    lv_anim_start(&anim);
}

static const char *action_label(const char *name)
{
    for(unsigned p = 0; p < 5; p++)
        for(unsigned i = 0; i < 3; i++)
            if(strcmp(pages[p][i], name) == 0) return page_labels[p][i];
    return UI_IDLE;
}

bool pet_ui_action(const char *name, bool notify)
{
    if(strcmp(name, "wake") == 0) { set_ambient(false); return true; }
    if(strcmp(name, "sleep") == 0) {
        clip = frame = 0;
        paused = false;
        lv_label_set_text(caption, UI_IDLE);
        refresh_labels();
        set_ambient(true);
        // Existing PC clients understand reset, not a new sleep action.
        if(notify && cb.notify_action) cb.notify_action("reset");
        return true;
    }
    int selected = -1;
    for(unsigned i = 0; i < PET_CLIP_COUNT; i++)
        if(strcmp(pet_clips[i].name, name) == 0) selected = (int)i;
    bool navigation = strcmp(name, "more") == 0 || strcmp(name, "home") == 0;
    bool pause = strcmp(name, "pause") == 0, reset = strcmp(name, "reset") == 0;
    bool chat = strcmp(name, "chat") == 0;
    if(selected < 0 && !navigation && !pause && !reset && !chat) return false;
    set_ambient(false);
    if(navigation) {
        page = strcmp(name, "home") == 0 ? 0 : (page + 1) % 5;
        refresh_labels();
        return true;
    }
    if(pause) paused = !paused;
    else if(reset || selected >= 0) {
        clip = reset ? 0 : (unsigned)selected;
        frame = 0;
        paused = false;
    }
    lv_label_set_text(caption, paused ? UI_REST : chat ?
                      (cb.is_live() ? UI_CHAT_LIVE : UI_CHAT_OFFLINE) :
                      pause ? UI_RESUME : action_label(name));
    refresh_labels();
    if(notify && cb.notify_action) cb.notify_action(name);
    return true;
}

static void button_event(lv_event_t *event)
{
    if(!ambient) pet_ui_action(pages[page][(uintptr_t)lv_event_get_user_data(event)], true);
}

static void touch_started(lv_event_t *event)
{
    (void)event;
    last_interaction = lv_tick_get();
}

static void pet_touch(lv_event_t *event)
{
    if(ambient) { set_ambient(false); return; }
    pet_ui_action(lv_event_get_code(event) == LV_EVENT_LONG_PRESSED ? "sleep" : "smile", true);
}

static void chat_touch(lv_event_t *event)
{
    (void)event;
    if(!ambient) pet_ui_action("chat", true);
}

static void animate(lv_timer_t *timer)
{
    (void)timer;
    bool live = cb.is_live();
    uint32_t now = lv_tick_get();
    if(live != was_live) {
        was_live = live;
        lv_label_set_text(status_text, live ? UI_LIVE : UI_OFFLINE);
        lv_obj_set_style_text_color(status_text, lv_color_hex(live ? ACCENT : MUTED), 0);
        // Finish a remote action cleanly on disconnect, including while paused.
        if(!live) { clip = frame = 0; cb.render_frame(0); }
    }
    // A long offline action (e.g. the 24s dance) finishes before its menu closes.
    if(!ambient && (live || clip == 0 || paused) && now - last_interaction >= PET_UI_IDLE_MS)
        set_ambient(true);
    if(ambient && !paused && !transitioning) {
        int32_t breath = lv_trigo_sin((now % 4400) * 360 / 4400);
        lv_image_set_scale(portrait, AMBIENT_SCALE + breath * 2 / 32768);
        lv_obj_set_y(portrait, 91 + breath * 2 / 32768);
        for(unsigned i = 0; i < 4; i++) {
            int32_t shimmer = lv_trigo_sin(((now + i * 1100) % 5200) * 360 / 5200);
            lv_obj_set_style_opa(spark[i], 100 + shimmer * 65 / 32768, 0);
        }
    }
    if(!live && !paused) {
        // Gentle, infrequent gestures only; never send unsolicited USB actions.
        if(ambient && clip == 0 && now - last_gesture >= 6500 + (gesture % 3) * 1700) {
            static const unsigned gestures[] = {2, 4, 2, 5, 3};
            clip = gestures[gesture++ % 5];
            frame = 0;
            last_gesture = now;
        }
        const pet_clip_t *c = &pet_clips[clip];
        cb.render_frame(c->first + frame);
        if(++frame >= c->count) {
            frame = 0;
            if(clip != 0) { clip = 0; lv_label_set_text(caption, UI_IDLE); }
        }
    }
}

static lv_obj_t *text(lv_obj_t *parent, const char *str, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, str);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

static lv_obj_t *decoration(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

void pet_ui_init(const lv_image_dsc_t *image, const pet_ui_callbacks_t *callbacks)
{
    cb = *callbacks;
    last_interaction = last_gesture = lv_tick_get();
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(screen, pet_touch, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(screen, pet_touch, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(screen, touch_started, LV_EVENT_PRESSED, NULL);

    // Put the black-backed image first so it never paints over labels.
    portrait = lv_image_create(screen);
    lv_image_set_src(portrait, image);
    lv_image_set_pivot(portrait, 160, 160);
    lv_obj_remove_flag(portrait, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *name = text(screen, UI_TITLE, &pet_font_zh_28, INK);
    lv_obj_set_style_text_letter_space(name, 3, 0);
    lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 26);
    subtitle = text(screen, UI_COMPANION, &pet_font_zh_16, MUTED);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 64);
    status_text = text(screen, UI_OFFLINE, &pet_font_zh_16, MUTED);
    lv_obj_align(status_text, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_set_ext_click_area(status_text, 14);
    lv_obj_add_flag(status_text, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_text, chat_touch, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(status_text, touch_started, LV_EVENT_PRESSED, NULL);

    hint = text(screen, UI_WAKE_HINT, &pet_font_zh_16, MUTED);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 428);
    caption = text(screen, UI_IDLE, &pet_font_zh_20, INK);
    lv_obj_set_width(caption, 300);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(caption, LV_LABEL_LONG_DOT);
    lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, 342);

    spark[0] = decoration(screen, 132, 171, 4, 4, ACCENT);
    spark[1] = decoration(screen, 325, 224, 5, 5, 0xB4C8ED);
    spark[2] = decoration(screen, 144, 306, 3, 3, 0xB4C8ED);
    spark[3] = decoration(screen, 310, 137, 3, 3, ACCENT);

    controls = lv_obj_create(screen);
    lv_obj_remove_style_all(controls);
    lv_obj_remove_flag(controls, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(controls, 94, 374);
    lv_obj_set_size(controls, 278, 67);
    for(unsigned i = 0; i < 3; i++) {
        btn[i] = lv_button_create(controls);
        lv_obj_set_pos(btn[i], i * 96, 0);
        lv_obj_set_size(btn[i], 86, 46);
        lv_obj_set_style_radius(btn[i], 19, 0);
        lv_obj_set_style_bg_color(btn[i], lv_color_hex(i == 0 ? ACCENT : SURFACE), 0);
        lv_obj_set_style_bg_color(btn[i], lv_color_hex(i == 0 ? 0xBBF4E5 : 0x39515B), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn[i], i == 0 ? 0 : 1, 0);
        lv_obj_set_style_border_color(btn[i], lv_color_hex(OUTLINE), 0);
        lv_obj_set_style_shadow_width(btn[i], 0, 0);
        // The stock theme grows pressed buttons; that clips the outer button on a round screen.
        lv_obj_set_style_transform_width(btn[i], 0, LV_STATE_PRESSED);
        lv_obj_set_style_transform_height(btn[i], 0, LV_STATE_PRESSED);
        lv_obj_set_style_pad_all(btn[i], 0, 0);
        btn_text[i] = text(btn[i], page_labels[0][i], &pet_font_zh_20, i == 0 ? 0x102B29 : INK);
        lv_obj_center(btn_text[i]);
        lv_obj_add_event_cb(btn[i], button_event, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(btn[i], touch_started, LV_EVENT_PRESSED, NULL);
    }
    for(unsigned i = 0; i < 5; i++)
        dots[i] = decoration(controls, 103 + i * 16, 59, 5, 4, OUTLINE);
    refresh_labels();
    visible(controls, false);
    visible(status_text, false);
    visible(caption, false);
    set_presentation(NULL, 0);
    cb.render_frame(0);
    lv_timer_create(animate, 125, NULL);
}

pet_ui_state_t pet_ui_get_state(void)
{
    return (pet_ui_state_t){.ambient = ambient, .paused = paused, .page = page,
                            .action = pet_clips[clip].name};
}

void pet_ui_invalidate_image(void) { lv_obj_invalidate(portrait); }

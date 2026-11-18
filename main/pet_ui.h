#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#define PET_UI_IDLE_MS 20000U

typedef struct {
    bool (*render_frame)(unsigned index);
    bool (*is_live)(void);
    void (*notify_action)(const char *name);
} pet_ui_callbacks_t;

typedef struct {
    bool ambient;
    bool paused;
    unsigned page;
    const char *action;
} pet_ui_state_t;

// All calls require the BSP/LVGL lock. Incoming video does not wake the UI.
void pet_ui_init(const lv_image_dsc_t *image, const pet_ui_callbacks_t *callbacks);
bool pet_ui_action(const char *name, bool notify);
pet_ui_state_t pet_ui_get_state(void);
void pet_ui_invalidate_image(void);

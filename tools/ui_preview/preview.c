// Render the firmware's real LVGL UI and exercise pointer input without a board.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "pet_ui.h"
#include "pet_assets.h"

static uint16_t pixels[320 * 320];
static uint8_t display_buffer[466 * 40 * 2];
static uint8_t *assets;
static bool live, pressed;
static lv_point_t point;
static unsigned notifications, rendered;
static char last_event[32];
static const char *output;
static lv_image_dsc_t image;

static bool render(unsigned index)
{
    assert(index < sizeof(pet_frames) / sizeof(pet_frames[0]));
    const pet_frame_t *frame = &pet_frames[index];
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_mgr error;
    decoder.err = jpeg_std_error(&error);
    jpeg_create_decompress(&decoder);
    jpeg_mem_src(&decoder, assets + frame->offset, frame->length);
    assert(jpeg_read_header(&decoder, TRUE) == JPEG_HEADER_OK);
    decoder.out_color_space = JCS_RGB;
    jpeg_start_decompress(&decoder);
    assert(decoder.output_width == 320 && decoder.output_height == 320);
    uint8_t row[320 * 3];
    while(decoder.output_scanline < 320) {
        unsigned y = decoder.output_scanline;
        JSAMPROW scanline = row;
        jpeg_read_scanlines(&decoder, &scanline, 1);
        for(unsigned x = 0; x < 320; x++)
            pixels[y * 320 + x] = ((row[x * 3] >> 3) << 11) |
                ((row[x * 3 + 1] >> 2) << 5) | (row[x * 3 + 2] >> 3);
    }
    jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    lv_image_cache_drop(&image);
    pet_ui_invalidate_image();
    rendered++;
    return true;
}

static bool is_live(void) { return live; }
static void notify(const char *name)
{
    notifications++;
    snprintf(last_event, sizeof(last_event), "%s", name);
}
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    (void)area; (void)data;
    lv_display_flush_ready(display);
}
static void read_pointer(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point = point;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void advance(unsigned ms)
{
    for(unsigned i = 0; i < ms; i += 5) {
        lv_tick_inc(5);
        lv_timer_handler();
    }
}
static void touch(int x, int y, unsigned hold_ms)
{
    point = (lv_point_t){x, y};
    pressed = true;
    advance(hold_ms);
    pressed = false;
    advance(80);
}
static void check_button_geometry(lv_obj_t *obj)
{
    if(lv_obj_check_type(obj, &lv_button_class)) {
        assert(lv_obj_get_style_transform_width(obj, 0) == 0);
        assert(lv_obj_get_style_transform_height(obj, 0) == 0);
    }
    for(unsigned i = 0; i < lv_obj_get_child_count(obj); i++)
        check_button_geometry(lv_obj_get_child(obj, i));
}
static void snapshot(const char *name)
{
    lv_obj_update_layout(lv_screen_active());
    lv_draw_buf_t *buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
    assert(buf && buf->header.w == 466 && buf->header.h == 466);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.ppm", output, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n466 466\n255\n");
    for(unsigned y = 0; y < 466; y++) {
        const uint16_t *row = (const uint16_t *)(buf->data + y * buf->header.stride);
        for(unsigned x = 0; x < 466; x++) {
            uint16_t p = row[x];
            uint8_t rgb[] = {((p >> 11) & 31) * 255 / 31, ((p >> 5) & 63) * 255 / 63, (p & 31) * 255 / 31};
            // Show the actual circular aperture, not the square framebuffer corners.
            int dx = (int)x - 233, dy = (int)y - 233;
            if(dx * dx + dy * dy > 233 * 233)
                memset(rgb, 22, sizeof(rgb));
            assert(fwrite(rgb, 1, sizeof(rgb), file) == sizeof(rgb));
        }
    }
    fclose(file);
    lv_draw_buf_destroy(buf);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    output = argv[2];
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    assert(size > 0);
    rewind(file);
    assets = malloc(size);
    assert(assets && fread(assets, 1, size, file) == (size_t)size);
    fclose(file);
    lv_init();
    lv_display_t *display = lv_display_create(466, 466);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, display_buffer, NULL, sizeof(display_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_indev_t *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, read_pointer);
    image = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
        .w = 320, .h = 320, .stride = 640}, .data_size = sizeof(pixels), .data = (uint8_t *)pixels};
    const pet_ui_callbacks_t callbacks = {render, is_live, notify};
    pet_ui_init(&image, &callbacks);
    advance(500);
    assert(pet_ui_get_state().ambient);
    snapshot("ambient");

    // A tap in the former button area must ONLY wake; it must not issue wave.
    touch(137, 395, 80);
    advance(400);
    assert(!pet_ui_get_state().ambient && notifications == 0);
    snapshot("awake");
    touch(137, 395, 80);
    assert(strcmp(last_event, "wave") == 0 && notifications == 1);
    touch(233, 220, 80);
    assert(strcmp(last_event, "smile") == 0 && notifications == 2);
    touch(233, 220, 700);
    advance(400);
    assert(pet_ui_get_state().ambient && strcmp(last_event, "reset") == 0 && notifications == 3);

    // A long press from rest also only wakes, and release never smiles.
    touch(233, 220, 700);
    assert(!pet_ui_get_state().ambient && notifications == 3);
    advance(400);
    for(unsigned page = 1; page < 5; page++) {
        point = (lv_point_t){329, 395};
        pressed = true;
        advance(200);
        check_button_geometry(lv_screen_active());
        if(page == 1) snapshot("button-pressed");
        pressed = false;
        advance(250);
        assert(pet_ui_get_state().page == page && notifications == 3);
        char name[32];
        snprintf(name, sizeof(name), "page-%u", page + 1);
        snapshot(name);
    }
    touch(233, 395, 80);
    assert(pet_ui_get_state().paused);
    unsigned count = rendered;
    advance(PET_UI_IDLE_MS + 500);
    assert(pet_ui_get_state().ambient && pet_ui_get_state().paused && rendered == count);
    touch(233, 220, 80);
    assert(!pet_ui_get_state().ambient && pet_ui_get_state().paused);
    // Long press still recovers from an explicit pause.
    touch(233, 220, 700);
    assert(pet_ui_get_state().ambient && !pet_ui_get_state().paused);

    count = notifications;
    bool saw_gesture = false;
    for(unsigned i = 0; i < 100; i++) {
        advance(125);
        if(strcmp(pet_ui_get_state().action, "idle") != 0) saw_gesture = true;
    }
    assert(saw_gesture && notifications == count && pet_ui_get_state().ambient);
    assert(!pet_ui_action("not-an-action", true));
    assert(pet_ui_get_state().ambient && notifications == count);

    pet_ui_action("grace", true);
    advance(PET_UI_IDLE_MS + 500);
    assert(!pet_ui_get_state().ambient); // Don't interrupt the 24s clip.
    advance(4500);
    assert(pet_ui_get_state().ambient);

    live = true;
    count = rendered;
    advance(25000);
    assert(pet_ui_get_state().ambient && rendered == count);
    pet_ui_action("chat", true);
    advance(400);
    snapshot("live-chat");
    advance(PET_UI_IDLE_MS + 500);
    assert(pet_ui_get_state().ambient);
    live = false;
    advance(500);
    assert(pet_ui_get_state().ambient && rendered > count);

    // Exercise reversals while the layout transition is in flight.
    for(unsigned i = 0; i < 10; i++) {
        pet_ui_action("wake", false); advance(50);
        pet_ui_action("sleep", false); advance(50);
    }
    advance(500);
    assert(pet_ui_get_state().ambient);
    snapshot("ambient-final");
    puts("PASS: wake/tap/long-press, five pages, pause, timeout, idle gestures, live fallback, transition reversals");
    lv_deinit();
    free(assets);
    return 0;
}

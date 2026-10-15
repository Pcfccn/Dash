/* Host-side stand-in for the bits of LVGL 9 st7735_status.c uses: widgets are
 * opaque handles, every write is counted. Implemented by
 * tests/host/test_st7735_status.c. */
#ifndef MOCK_ST_LVGL_H
#define MOCK_ST_LVGL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct lv_display_t lv_display_t;
typedef struct lv_obj_t     lv_obj_t;
typedef struct { int unused; } lv_font_t;
typedef struct { uint8_t r, g, b; } lv_color_t;

#define LV_FONT_DECLARE(name) extern const lv_font_t name
extern const lv_font_t lv_font_montserrat_12, lv_font_montserrat_14;

#define LV_UNUSED(x)                   ((void)(x))
#define LV_OPA_COVER                   255
#define LV_DISPLAY_RENDER_MODE_PARTIAL 0
#define LV_OBJ_FLAG_HIDDEN             0x1u
#define LV_TEXT_ALIGN_CENTER           2
enum { LV_ALIGN_TOP_LEFT = 1, LV_ALIGN_CENTER = 9 };

lv_display_t *lv_display_get_default(void);
void          lv_display_set_default(lv_display_t *d);
void          lv_display_set_buffers(lv_display_t *d, void *b1, void *b2, uint32_t size, int mode);
lv_obj_t     *lv_display_get_screen_active(lv_display_t *d);
void          lv_display_flush_ready(lv_display_t *d);
uint32_t      lv_tick_get(void);
void          lv_lock(void);
void          lv_unlock(void);

lv_color_t lv_color_black(void);
lv_color_t lv_color_white(void);
lv_color_t lv_color_hex(uint32_t c);

lv_obj_t *lv_obj_create(lv_obj_t *parent);
lv_obj_t *lv_label_create(lv_obj_t *parent);
void      lv_label_set_text(lv_obj_t *o, const char *t);
void      lv_label_set_text_fmt(lv_obj_t *o, const char *fmt, ...);
void      lv_obj_remove_style_all(lv_obj_t *o);
void      lv_obj_set_size(lv_obj_t *o, int32_t w, int32_t h);
void      lv_obj_set_width(lv_obj_t *o, int32_t w);
void      lv_obj_align(lv_obj_t *o, int align, int32_t x, int32_t y);
void      lv_obj_add_flag(lv_obj_t *o, uint32_t f);
void      lv_obj_clear_flag(lv_obj_t *o, uint32_t f);
void      lv_obj_invalidate(const lv_obj_t *o);
void      lv_obj_set_style_bg_color(lv_obj_t *o, lv_color_t c, uint32_t sel);
void      lv_obj_set_style_bg_opa(lv_obj_t *o, uint8_t v, uint32_t sel);
void      lv_obj_set_style_pad_all(lv_obj_t *o, int32_t v, uint32_t sel);
void      lv_obj_set_style_text_font(lv_obj_t *o, const lv_font_t *f, uint32_t sel);
void      lv_obj_set_style_text_color(lv_obj_t *o, lv_color_t c, uint32_t sel);
void      lv_obj_set_style_text_align(lv_obj_t *o, int a, uint32_t sel);

#endif

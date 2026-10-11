/* Host-side stand-in for the bits of LVGL 9 lv_port_disp.c uses. Implemented
 * by tests/host/test_lv_port_disp.c. */
#ifndef MOCK_LVGL_H
#define MOCK_LVGL_H

#include <stdint.h>

typedef struct { int32_t x1, y1, x2, y2; } lv_area_t;
typedef struct lv_display_t lv_display_t;
typedef struct lv_obj_t     lv_obj_t;
typedef void     (*lv_display_flush_cb_t)(lv_display_t *d, const lv_area_t *a, uint8_t *px);
typedef uint32_t (*lv_tick_get_cb_t)(void);

#define LV_COLOR_FORMAT_RGB565         0x12
#define LV_DISPLAY_RENDER_MODE_PARTIAL 0

static inline int32_t lv_area_get_width(const lv_area_t *a)  { return a->x2 - a->x1 + 1; }
static inline int32_t lv_area_get_height(const lv_area_t *a) { return a->y2 - a->y1 + 1; }

void          lv_init(void);
void          lv_tick_set_cb(lv_tick_get_cb_t cb);
lv_display_t *lv_display_create(int32_t w, int32_t h);
void          lv_display_set_color_format(lv_display_t *d, int cf);
void          lv_display_set_flush_cb(lv_display_t *d, lv_display_flush_cb_t cb);
void          lv_display_set_buffers(lv_display_t *d, void *b1, void *b2, uint32_t size, int mode);
void          lv_display_flush_ready(lv_display_t *d);
lv_obj_t     *lv_display_get_screen_active(lv_display_t *d);
void          lv_obj_invalidate(const lv_obj_t *o);

#endif

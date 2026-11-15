#ifndef KS_UI_D2D_RENDERER_H
#define KS_UI_D2D_RENDERER_H

#include "ui/theme.h"

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KsD2DRenderer KsD2DRenderer;

typedef enum {
    KS_ALIGN_LEFT = 0,
    KS_ALIGN_CENTER = 1,
    KS_ALIGN_RIGHT = 2
} KsTextAlign;

typedef enum {
    KS_FONT_REGULAR = 0,
    KS_FONT_MEDIUM = 1,
    KS_FONT_SEMIBOLD = 2,
    KS_FONT_BOLD = 3
} KsFontWeight;

typedef struct {
    char **names;
    int count;
    int preset_count;
} KsFontList;

KsD2DRenderer *ks_d2d_create(HWND hwnd);
void ks_d2d_destroy(KsD2DRenderer *r);

bool ks_d2d_resize(KsD2DRenderer *r, uint32_t width_px, uint32_t height_px, float dpi);
bool ks_d2d_begin_draw(KsD2DRenderer *r, KsColor clear_color);
bool ks_d2d_end_draw(KsD2DRenderer *r);

/* Releases cached offscreen surfaces and trims DXGI/process working set (especially when minimized to tray). */
void ks_d2d_trim_memory(KsD2DRenderer *r, bool release_scene_buffers);

/* Apply Windows 11 Mica / borderless rounded chrome + Win10 fallback backdrop. */
bool ks_d2d_apply_window_backdrop(HWND hwnd, bool dark_mode, bool *out_mica_active);
bool ks_d2d_is_mica_supported(void);

/* Configure active UI font family (empty or NULL falls back cleanly). */
void ks_d2d_set_font_family(KsD2DRenderer *r, const char *font_family_utf8);

/* System font discovery (12 presets first, then deduplicated sorted system fonts). */
void ks_d2d_enumerate_fonts(KsD2DRenderer *r, KsFontList *out_list);
void ks_d2d_free_font_list(KsFontList *list);

/* Geometry & clipping primitives (all coordinates in DIPs). */
void ks_d2d_push_clip(KsD2DRenderer *r, float x, float y, float w, float h, float radius);
void ks_d2d_pop_clip(KsD2DRenderer *r);

void ks_d2d_fill_rect(KsD2DRenderer *r, float x, float y, float w, float h, KsColor color);
void ks_d2d_fill_rounded_rect(KsD2DRenderer *r, float x, float y, float w, float h, float radius, KsColor color);
void ks_d2d_stroke_rounded_rect(KsD2DRenderer *r, float x, float y, float w, float h, float radius,
                                float stroke_width, KsColor color);
void ks_d2d_fill_rounded_gradient_v(KsD2DRenderer *r, float x, float y, float w, float h, float radius,
                                    KsColor top_color, KsColor bottom_color);
void ks_d2d_fill_radial_glow(KsD2DRenderer *r, float cx, float cy, float rx, float ry, KsColor center_color);
void ks_d2d_fill_circle(KsD2DRenderer *r, float cx, float cy, float radius, KsColor color);
void ks_d2d_stroke_circle(KsD2DRenderer *r, float cx, float cy, float radius, float stroke_width, KsColor color);
void ks_d2d_draw_line(KsD2DRenderer *r, float x1, float y1, float x2, float y2, float width, KsColor color);

void ks_d2d_draw_soft_shadow(KsD2DRenderer *r, float x, float y, float w, float h, float radius,
                              KsColor shadow_color, float spread, float offset_y);

/* Captures the scene behind (x, y, w, h), applies CLSID_D2D1GaussianBlur,
 * clips to rounded rect, and composites tinted glass + inner border highlight. */
void ks_d2d_draw_glass_panel(KsD2DRenderer *r, float x, float y, float w, float h, float radius,
                             float blur_sigma, KsColor tint, KsColor border_color, KsColor shadow_color);

/* Crisp vector icons matching original SVG shapes. */
void ks_d2d_draw_app_badge(KsD2DRenderer *r, float x, float y, float size, KsColor accent, KsColor icon_fg);
void ks_d2d_draw_chevron_down(KsD2DRenderer *r, float cx, float cy, float size, KsColor color, bool flipped);
void ks_d2d_draw_dots_vertical(KsD2DRenderer *r, float cx, float cy, float size, KsColor color);
void ks_d2d_draw_close_x(KsD2DRenderer *r, float cx, float cy, float size, KsColor color);
void ks_d2d_draw_checkmark(KsD2DRenderer *r, float cx, float cy, float size, KsColor color);
void ks_d2d_draw_minimize_dash(KsD2DRenderer *r, float cx, float cy, float size, KsColor color);
void ks_d2d_draw_maximize_box(KsD2DRenderer *r, float cx, float cy, float size, KsColor color, bool is_maximized);
void ks_d2d_draw_menu_icon(KsD2DRenderer *r, int icon_kind, float cx, float cy, float size, KsColor color);

/* DirectWrite text drawing and measurement. */
void ks_d2d_draw_text(KsD2DRenderer *r, const char *utf8_text, float x, float y, float w, float h,
                      float font_size, KsFontWeight weight, KsColor color,
                      KsTextAlign h_align, bool v_center, bool ellipsis, const char *override_font_utf8);
float ks_d2d_measure_text_width(KsD2DRenderer *r, const char *utf8_text, float font_size,
                                KsFontWeight weight, const char *override_font_utf8);

/* Saves the last committed scene to a PNG file via WIC (used by visual verification). */
bool ks_d2d_save_snapshot_png_w(KsD2DRenderer *r, const wchar_t *path_w);

#ifdef __cplusplus
}
#endif

#endif

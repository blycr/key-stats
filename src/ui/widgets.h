#ifndef KS_UI_WIDGETS_H
#define KS_UI_WIDGETS_H

#include "ui/d2d_renderer.h"
#include "ui/theme.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float x;
    float y;
    float w;
    float h;
} KsRect;

static inline bool ks_rect_contains(KsRect r, float mx, float my) {
    return mx >= r.x && mx <= (r.x + r.w) && my >= r.y && my <= (r.y + r.h);
}

void ks_format_count(int64_t val, char *buf, int cap);
void ks_format_compact_count(int64_t val, char *buf, int cap);

typedef enum {
    KS_BTN_SECONDARY = 0,
    KS_BTN_PRIMARY = 1,
    KS_BTN_DANGER = 2,
    KS_BTN_GHOST = 3
} KsButtonVariant;

void ks_draw_button(KsD2DRenderer *r, const KsThemePalette *pal, KsRect rect,
                    const char *label, KsButtonVariant variant,
                    bool hovered, bool pressed, float font_size, float radius);

void ks_draw_toggle_switch(KsD2DRenderer *r, const KsThemePalette *pal,
                           float x, float y, bool checked, bool hovered);

void ks_draw_progress_bar(KsD2DRenderer *r, const KsThemePalette *pal,
                          KsRect rect, float fraction, KsColor bar_color);

void ks_draw_modal_scrim(KsD2DRenderer *r, float win_w, float win_h, float alpha);

#endif

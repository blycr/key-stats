#include "ui/widgets.h"

#include <stdio.h>
#include <string.h>

void ks_format_count(int64_t val, char *buf, int cap) {
    if (!buf || cap <= 0) return;
    if (val < 0) val = 0;

    char raw[32];
    snprintf(raw, sizeof(raw), "%lld", (long long)val);
    int len = (int)strlen(raw);
    char out[48];
    int j = 0;
    for (int i = 0; i < len; ++i) {
        if (i > 0 && ((len - i) % 3 == 0)) {
            out[j++] = ',';
        }
        out[j++] = raw[i];
    }
    out[j] = '\0';
    snprintf(buf, (size_t)cap, "%s", out);
}

void ks_format_compact_count(int64_t val, char *buf, int cap) {
    if (!buf || cap <= 0) return;
    if (val < 0) val = 0;
    if (val >= 1000000) {
        snprintf(buf, (size_t)cap, "%.1fM", (double)val / 1000000.0);
    } else if (val >= 10000) {
        snprintf(buf, (size_t)cap, "%.1fk", (double)val / 1000.0);
    } else {
        snprintf(buf, (size_t)cap, "%lld", (long long)val);
    }
}

void ks_draw_button(KsD2DRenderer *r, const KsThemePalette *pal, KsRect rect,
                    const char *label, KsButtonVariant variant,
                    bool hovered, bool pressed, float font_size, float radius) {
    KsColor bg = pal->surface_overlay;
    KsColor fg = pal->text_primary;

    if (variant == KS_BTN_PRIMARY) {
        bg = hovered ? pal->accent_hover : pal->accent;
        fg = ks_rgba(255, 255, 255, 1.0f);
    } else if (variant == KS_BTN_DANGER) {
        bg = hovered ? ks_color_lerp(pal->danger, ks_rgba(255, 255, 255, 1.0f), 0.14f) : pal->danger;
        fg = ks_rgba(255, 255, 255, 1.0f);
    } else if (variant == KS_BTN_GHOST) {
        bg = hovered ? pal->surface_overlay : ks_rgba(0, 0, 0, 0.0f);
    } else {
        if (hovered) {
            bg = ks_color_lerp(pal->surface_overlay, pal->text_primary, 0.08f);
        }
    }

    if (pressed) {
        bg = ks_color_lerp(bg, ks_rgba(0, 0, 0, 1.0f), 0.12f);
    }

    if (bg.a > 0.001f) {
        ks_d2d_fill_rounded_rect(r, rect.x, rect.y, rect.w, rect.h, radius, bg);
    }
    if (label && label[0] != '\0') {
        ks_d2d_draw_text(r, label, rect.x + 8.0f, rect.y, rect.w - 16.0f, rect.h,
                         font_size, KS_FONT_MEDIUM, fg, KS_ALIGN_CENTER, true, true, NULL);
    }
}

void ks_draw_toggle_switch(KsD2DRenderer *r, const KsThemePalette *pal,
                           float x, float y, bool checked, bool hovered) {
    float w = 40.0f;
    float h = 22.0f;
    KsColor track = checked ? (hovered ? pal->accent_hover : pal->accent)
                            : (hovered ? ks_color_lerp(pal->surface_overlay, pal->text_primary, 0.08f) : pal->surface_overlay);
    ks_d2d_fill_rounded_rect(r, x, y, w, h, h * 0.5f, track);
    float knob_r = 8.0f;
    float knob_cx = checked ? (x + w - 11.0f) : (x + 11.0f);
    float knob_cy = y + h * 0.5f;
    ks_d2d_fill_circle(r, knob_cx, knob_cy, knob_r, ks_rgba(255, 255, 255, 0.96f));
}

void ks_draw_progress_bar(KsD2DRenderer *r, const KsThemePalette *pal,
                          KsRect rect, float fraction, KsColor bar_color) {
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    float rad = rect.h * 0.5f;
    ks_d2d_fill_rounded_rect(r, rect.x, rect.y, rect.w, rect.h, rad, pal->border_subtle);
    float fill_w = rect.w * fraction;
    if (fill_w > 0.0f) {
        if (fill_w < rect.h && fraction > 0.0f) fill_w = rect.h;
        if (fill_w > rect.w) fill_w = rect.w;
        ks_d2d_fill_rounded_rect(r, rect.x, rect.y, fill_w, rect.h, rad, bar_color);
    }
}

void ks_draw_modal_scrim(KsD2DRenderer *r, float win_w, float win_h, float alpha) {
    if (alpha <= 0.001f) return;
    ks_d2d_fill_rect(r, 0.0f, 0.0f, win_w, win_h, ks_rgba(0, 0, 0, 0.48f * alpha));
}

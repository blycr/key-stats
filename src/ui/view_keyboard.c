#include "ui/view_keyboard.h"

#include <stdio.h>
#include <string.h>

static const KsKeyCapDef kRow0[] = {
    { "Esc",  "Esc",    NULL, 1.25f },
    { "F1",   "F1",     NULL, 1.0f },
    { "F2",   "F2",     NULL, 1.0f },
    { "F3",   "F3",     NULL, 1.0f },
    { "F4",   "F4",     NULL, 1.0f },
    { "F5",   "F5",     NULL, 1.0f },
    { "F6",   "F6",     NULL, 1.0f },
    { "F7",   "F7",     NULL, 1.0f },
    { "F8",   "F8",     NULL, 1.0f },
    { "F9",   "F9",     NULL, 1.0f },
    { "F10",  "F10",    NULL, 1.0f },
    { "F11",  "F11",    NULL, 1.0f },
    { "F12",  "F12",    NULL, 1.0f },
    { "Del",  "Delete", "Del", 1.35f }
};

static const KsKeyCapDef kRow1[] = {
    { "`",         "`",    "~",         1.0f },
    { "1",         "1",    NULL,        1.0f },
    { "2",         "2",    NULL,        1.0f },
    { "3",         "3",    NULL,        1.0f },
    { "4",         "4",    NULL,        1.0f },
    { "5",         "5",    NULL,        1.0f },
    { "6",         "6",    NULL,        1.0f },
    { "7",         "7",    NULL,        1.0f },
    { "8",         "8",    NULL,        1.0f },
    { "9",         "9",    NULL,        1.0f },
    { "0",         "0",    NULL,        1.0f },
    { "-",         "-",    NULL,        1.0f },
    { "=",         "=",    NULL,        1.0f },
    { "Backspace", "Back", "Backspace", 2.0f }
};

static const KsKeyCapDef kRow2[] = {
    { "Tab", "Tab", NULL, 1.5f },
    { "Q",   "Q",   NULL, 1.0f },
    { "W",   "W",   NULL, 1.0f },
    { "E",   "E",   NULL, 1.0f },
    { "R",   "R",   NULL, 1.0f },
    { "T",   "T",   NULL, 1.0f },
    { "Y",   "Y",   NULL, 1.0f },
    { "U",   "U",   NULL, 1.0f },
    { "I",   "I",   NULL, 1.0f },
    { "O",   "O",   NULL, 1.0f },
    { "P",   "P",   NULL, 1.0f },
    { "[",   "[",   NULL, 1.0f },
    { "]",   "]",   NULL, 1.0f },
    { "\\",  "\\",  NULL, 1.5f }
};

static const KsKeyCapDef kRow3[] = {
    { "Caps",  "Caps",  NULL,     1.75f },
    { "A",     "A",     NULL,     1.0f },
    { "S",     "S",     NULL,     1.0f },
    { "D",     "D",     NULL,     1.0f },
    { "F",     "F",     NULL,     1.0f },
    { "G",     "G",     NULL,     1.0f },
    { "H",     "H",     NULL,     1.0f },
    { "J",     "J",     NULL,     1.0f },
    { "K",     "K",     NULL,     1.0f },
    { "L",     "L",     NULL,     1.0f },
    { ";",     ";",     NULL,     1.0f },
    { "'",     "'",     NULL,     1.0f },
    { "Enter", "Enter", "Return", 2.25f }
};

static const KsKeyCapDef kRow4[] = {
    { "Shift", "LShift", "Shift", 2.25f },
    { "Z",     "Z",      NULL,    1.0f },
    { "X",     "X",      NULL,    1.0f },
    { "C",     "C",      NULL,    1.0f },
    { "V",     "V",      NULL,    1.0f },
    { "B",     "B",      NULL,    1.0f },
    { "N",     "N",      NULL,    1.0f },
    { "M",     "M",      NULL,    1.0f },
    { ",",     ",",      NULL,    1.0f },
    { ".",     ".",      NULL,    1.0f },
    { "/",     "/",      NULL,    1.0f },
    { "Shift", "RShift", NULL,    2.75f }
};

static const KsKeyCapDef kRow5[] = {
    { "Ctrl",  "LCtrl", "Ctrl",  1.35f },
    { "Win",   "LWin",  "RWin",  1.25f },
    { "Alt",   "LAlt",  "Alt",   1.25f },
    { "Space", "Space", NULL,    6.15f },
    { "Alt",   "RAlt",  NULL,    1.25f },
    { "Fn",    "Fn",    NULL,    1.25f },
    { "Ctrl",  "RCtrl", NULL,    1.5f }
};

typedef struct {
    const KsKeyCapDef *keys;
    int len;
} KsRowSlice;

static const KsRowSlice kRows[6] = {
    { kRow0, (int)(sizeof(kRow0) / sizeof(kRow0[0])) },
    { kRow1, (int)(sizeof(kRow1) / sizeof(kRow1[0])) },
    { kRow2, (int)(sizeof(kRow2) / sizeof(kRow2[0])) },
    { kRow3, (int)(sizeof(kRow3) / sizeof(kRow3[0])) },
    { kRow4, (int)(sizeof(kRow4) / sizeof(kRow4[0])) },
    { kRow5, (int)(sizeof(kRow5) / sizeof(kRow5[0])) }
};

int ks_keyboard_row_count(void) {
    return 6;
}

int ks_keyboard_row_len(int row) {
    if (row < 0 || row >= 6) return 0;
    return kRows[row].len;
}

const KsKeyCapDef *ks_keyboard_row_keys(int row) {
    if (row < 0 || row >= 6) return NULL;
    return kRows[row].keys;
}

static int64_t find_key_stat(const KsTodaySummary *summary, const char *name) {
    if (!summary || !name) return 0;
    for (int i = 0; i < summary->all_keys_len; ++i) {
        if (_stricmp(summary->all_keys[i].key_name, name) == 0) {
            return summary->all_keys[i].count;
        }
    }
    return 0;
}

int64_t ks_keyboard_lookup_count(const KsTodaySummary *summary, const KsKeyCapDef *def) {
    if (!summary || !def) return 0;
    int64_t total = find_key_stat(summary, def->primary_name);
    if (def->alias_name) {
        total += find_key_stat(summary, def->alias_name);
    }
    return total;
}

bool ks_keyboard_is_key_active(const KsKeyCapDef *def, char (*active_names)[KS_KEY_NAME_CAP], int active_count) {
    if (!def || !active_names || active_count <= 0) return false;
    for (int i = 0; i < active_count; ++i) {
        if (_stricmp(active_names[i], def->primary_name) == 0) return true;
        if (def->alias_name && _stricmp(active_names[i], def->alias_name) == 0) return true;
    }
    return false;
}

KsHeatTier ks_keyboard_heat_tier(int64_t count, int64_t max_count) {
    if (count <= 0 || max_count <= 0) return KS_HEAT_NONE;
    double ratio = (double)count / (double)max_count;
    if (ratio < 0.20) return KS_HEAT_LOW;
    if (ratio < 0.60) return KS_HEAT_MID;
    return KS_HEAT_HIGH;
}

int ks_view_keyboard_draw(KsD2DRenderer *r, const KsThemePalette *pal, KsLang lang,
                          KsRect card_rect, const KsTodaySummary *summary,
                          char (*active_names)[KS_KEY_NAME_CAP], int active_count,
                          float mouse_x, float mouse_y, bool allow_hover,
                          char *out_hover_tooltip, int tooltip_cap) {
    if (out_hover_tooltip && tooltip_cap > 0) out_hover_tooltip[0] = '\0';

    /* Borderless translucent surface */
    ks_d2d_fill_rounded_rect(r, card_rect.x, card_rect.y, card_rect.w, card_rect.h, 16.0f,
                             pal->surface);

    /* Card Header: Left Section Label + Right Borderless Heat Swatches */
    float pad = 22.0f;
    float header_y = card_rect.y + 18.0f;
    ks_d2d_draw_text(r, ks_tr(lang, KS_STR_KEYBOARD_HEATMAP),
                     card_rect.x + pad, header_y, card_rect.w * 0.50f, 22.0f,
                     13.0f, KS_FONT_SEMIBOLD, pal->text_secondary, KS_ALIGN_LEFT, true, true, NULL);

    const char *sub_lbl = ks_tr(lang, KS_STR_HEAT_SUBTITLE);
    float sub_w = ks_d2d_measure_text_width(r, sub_lbl, 11.0f, KS_FONT_REGULAR, NULL);
    float dots_w = 4.0f * 9.0f + 3.0f * 5.0f;
    float lx = card_rect.x + card_rect.w - pad - dots_w - 8.0f - sub_w;
    ks_d2d_draw_text(r, sub_lbl, lx, header_y, sub_w + 4.0f, 22.0f,
                     11.0f, KS_FONT_REGULAR, pal->text_tertiary, KS_ALIGN_LEFT, true, false, NULL);

    float sx = lx + sub_w + 8.0f;
    float sy = header_y + 6.5f;
    for (int step = 0; step < 4; ++step) {
        KsColor swatch = pal->surface_raised;
        if (step == 1) swatch = ks_color_lerp(pal->surface_raised, pal->heatmap, 0.24f);
        else if (step == 2) swatch = ks_color_lerp(pal->surface_raised, pal->heatmap, 0.56f);
        else if (step == 3) swatch = pal->heatmap;
        ks_d2d_fill_rounded_rect(r, sx, sy, 9.0f, 9.0f, 3.0f, swatch);
        sx += 14.0f;
    }

    /* Compute max key count for heatmap normalization */
    int64_t max_count = 0;
    int64_t total_keys = summary ? summary->total_keys : 0;
    if (summary && summary->all_keys_len > 0) {
        for (int row = 0; row < 6; ++row) {
            const KsKeyCapDef *keys = kRows[row].keys;
            int len = kRows[row].len;
            for (int i = 0; i < len; ++i) {
                int64_t c = ks_keyboard_lookup_count(summary, &keys[i]);
                if (c > max_count) max_count = c;
            }
        }
    }

    /* Borderless keyboard grid floating cleanly inside card body (zero nested chassis frame) */
    float body_x = card_rect.x + pad;
    float body_y = card_rect.y + 52.0f;
    float body_w = card_rect.w - pad * 2.0f;
    float body_h = card_rect.h - 52.0f - pad;
    if (body_w < 140.0f || body_h < 140.0f) return -1;

    float gap_x = (body_w > 640.0f) ? 8.0f : 5.5f;
    float gap_y = (body_h > 340.0f) ? 9.0f : 6.0f;

    float max_grid_w = body_w;
    float max_grid_h = body_h - (total_keys == 0 ? 24.0f : 0.0f);
    if (max_grid_h < 100.0f) max_grid_h = body_h;

    float row_h = (max_grid_h - gap_y * 5.0f) / 6.0f;
    float max_row_h_from_w = (max_grid_w / 15.0f) * 1.24f;
    if (row_h > max_row_h_from_w) row_h = max_row_h_from_w;
    if (row_h > 64.0f) row_h = 64.0f;

    float total_kb_h = row_h * 6.0f + gap_y * 5.0f;
    float grid_x = body_x;
    float grid_w = body_w;
    float start_y = body_y + (body_h - total_kb_h - (total_keys == 0 ? 18.0f : 0.0f)) * 0.5f;
    if (start_y < body_y) start_y = body_y;

    int hovered_global_idx = -1;
    int hovered_row = -1;
    KsRect hovered_rect = { 0 };
    char hovered_tip_text[96];
    hovered_tip_text[0] = '\0';

    int global_idx = 0;
    for (int row = 0; row < 6; ++row) {
        const KsKeyCapDef *keys = kRows[row].keys;
        int len = kRows[row].len;
        float sum_units = 0.0f;
        for (int i = 0; i < len; ++i) sum_units += keys[i].units;

        float avail_w = grid_w - gap_x * (float)(len - 1);
        float kx = grid_x;
        float ky = start_y + (float)row * (row_h + gap_y);
        float kh = (row == 0) ? (row_h * 0.85f) : row_h;
        if (row == 0) ky += (row_h - kh) * 0.5f;
        float radius = (kh >= 42.0f) ? 8.5f : 6.5f;

        for (int i = 0; i < len; ++i, ++global_idx) {
            float kw = avail_w * (keys[i].units / sum_units);
            KsRect kr = { kx, ky, kw, kh };
            kx += kw + gap_x;

            int64_t count = ks_keyboard_lookup_count(summary, &keys[i]);
            bool active = ks_keyboard_is_key_active(&keys[i], active_names, active_count);
            bool hovered = allow_hover && ks_rect_contains(kr, mouse_x, mouse_y);
            if (hovered) {
                hovered_global_idx = global_idx;
                hovered_row = row;
                hovered_rect = kr;
                char cnt_buf[32];
                ks_format_count(count, cnt_buf, sizeof(cnt_buf));
                snprintf(hovered_tip_text, sizeof(hovered_tip_text), "%s  ·  %s %s",
                         keys[i].label, cnt_buf, ks_tr(lang, KS_STR_TOOLTIP_PRESSES));
                if (out_hover_tooltip && tooltip_cap > 0) {
                    snprintf(out_hover_tooltip, (size_t)tooltip_cap, "%s", hovered_tip_text);
                }
            }

            KsHeatTier tier = ks_keyboard_heat_tier(count, max_count);
            KsColor fill = pal->surface_raised;
            KsColor text_col = pal->text_primary;

            if (active) {
                fill = pal->accent;
                text_col = ks_rgba(255, 255, 255, 0.99f);
            } else if (tier == KS_HEAT_LOW) {
                fill = ks_color_lerp(pal->surface_raised, pal->heatmap, 0.18f);
            } else if (tier == KS_HEAT_MID) {
                fill = ks_color_lerp(pal->surface_raised, pal->heatmap, 0.44f);
            } else if (tier == KS_HEAT_HIGH) {
                fill = ks_color_lerp(pal->surface_raised, pal->heatmap, 0.80f);
                text_col = ks_rgba(255, 255, 255, 0.98f);
            } else {
                text_col = pal->text_secondary;
            }

            if (hovered && !active) {
                fill = ks_color_lerp(fill, pal->text_primary, pal->dark_mode ? 0.12f : 0.07f);
                if (tier == KS_HEAT_NONE) text_col = pal->text_primary;
            }

            ks_d2d_fill_rounded_rect(r, kr.x, kr.y, kr.w, kr.h, radius, fill);

            /* Clean single centered key label */
            float lbl_size = (strlen(keys[i].label) > 2) ? (kh < 38.0f ? 9.5f : 11.0f)
                                                         : (kh < 38.0f ? 11.0f : 12.5f);
            ks_d2d_draw_text(r, keys[i].label, kr.x + 4.0f, kr.y, kr.w - 8.0f, kr.h,
                             lbl_size, (active || tier == KS_HEAT_HIGH) ? KS_FONT_SEMIBOLD : KS_FONT_MEDIUM,
                             text_col, KS_ALIGN_CENTER, true, true, NULL);
        }
    }

    /* Gentle bottom hint when no keys recorded yet */
    if (total_keys == 0 && hovered_global_idx < 0) {
        float hint_y = start_y + total_kb_h + 10.0f;
        if (hint_y + 18.0f <= card_rect.y + card_rect.h - 4.0f) {
            ks_d2d_draw_text(r, ks_tr(lang, KS_STR_AWAITING_KEYS),
                             body_x, hint_y, body_w, 18.0f,
                             11.5f, KS_FONT_REGULAR, pal->text_tertiary, KS_ALIGN_CENTER, true, false, NULL);
        }
    }

    /* Borderless floating glass hover tooltip */
    if (hovered_global_idx >= 0 && hovered_tip_text[0] != '\0') {
        float tw = ks_d2d_measure_text_width(r, hovered_tip_text, 12.0f, KS_FONT_SEMIBOLD, NULL) + 24.0f;
        float th = 28.0f;
        float tx = hovered_rect.x + hovered_rect.w * 0.5f - tw * 0.5f;
        float ty = (hovered_row == 0) ? (hovered_rect.y + hovered_rect.h + 8.0f)
                                      : (hovered_rect.y - th - 8.0f);
        if (tx < card_rect.x + 12.0f) tx = card_rect.x + 12.0f;
        if (tx + tw > card_rect.x + card_rect.w - 12.0f) tx = card_rect.x + card_rect.w - 12.0f - tw;

        ks_d2d_fill_rounded_rect(r, tx, ty, tw, th, 9.0f, pal->glass_tint);
        ks_d2d_draw_text(r, hovered_tip_text, tx + 8.0f, ty, tw - 16.0f, th,
                         12.0f, KS_FONT_SEMIBOLD, pal->text_primary, KS_ALIGN_CENTER, true, true, NULL);
    }

    return hovered_global_idx;
}

#include "ui/view_settings.h"
#include "core/win32_util.h"

#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool str_icontains(const char *hay, const char *needle) {
    if (!needle || needle[0] == '\0') return true;
    if (!hay) return false;
    size_t nlen = strlen(needle);
    for (const char *p = hay; *p != '\0'; ++p) {
        if (_strnicmp(p, needle, nlen) == 0) return true;
    }
    return false;
}

static KsRect compute_modal_rect(float win_w, float win_h) {
    float mw = 540.0f;
    float mh = 548.0f;
    if (mw > win_w - 36.0f) mw = win_w - 36.0f;
    if (mh > win_h - 36.0f) mh = win_h - 36.0f;
    return (KsRect){ (win_w - mw) * 0.5f, (win_h - mh) * 0.5f, mw, mh };
}

static KsRect compute_confirm_rect(float win_w, float win_h) {
    float cw = 416.0f;
    float ch = 204.0f;
    if (cw > win_w - 48.0f) cw = win_w - 48.0f;
    if (ch > win_h - 48.0f) ch = win_h - 48.0f;
    return (KsRect){ (win_w - cw) * 0.5f, (win_h - ch) * 0.5f, cw, ch };
}

static void destroy_search_edit(KsSettingsModal *m) {
    if (m->search_edit) {
        DestroyWindow(m->search_edit);
        m->search_edit = NULL;
    }
    if (m->search_hfont) {
        DeleteObject(m->search_hfont);
        m->search_hfont = NULL;
    }
    if (m->search_bg_brush) {
        DeleteObject(m->search_bg_brush);
        m->search_bg_brush = NULL;
    }
}

void ks_settings_init(KsSettingsModal *m) {
    memset(m, 0, sizeof(*m));
}

void ks_settings_destroy(KsSettingsModal *m) {
    if (!m) return;
    destroy_search_edit(m);
    free(m->filtered_idx);
    memset(m, 0, sizeof(*m));
}

void ks_settings_visible_range(int total_items, float scroll_y, float view_h, float item_h,
                               int *out_start, int *out_end) {
    if (total_items <= 0 || item_h <= 0.0f) {
        if (out_start) *out_start = 0;
        if (out_end) *out_end = -1;
        return;
    }
    int first = (int)(scroll_y / item_h);
    if (first < 0) first = 0;
    int visible = (int)(view_h / item_h) + 2;
    int last = first + visible;
    if (last >= total_items) last = total_items - 1;
    if (out_start) *out_start = first;
    if (out_end) *out_end = last;
}

void ks_settings_filter_fonts(KsSettingsModal *m, const KsFontList *fonts, const char *query_utf8) {
    if (!m) return;
    if (query_utf8) {
        snprintf(m->font_query, sizeof(m->font_query), "%s", query_utf8);
    } else {
        m->font_query[0] = '\0';
    }

    int max_entries = 1 + (fonts ? fonts->count : 0);
    if (max_entries > m->filtered_cap) {
        int *n = (int *)realloc(m->filtered_idx, (size_t)max_entries * sizeof(int));
        if (!n) return;
        m->filtered_idx = n;
        m->filtered_cap = max_entries;
    }

    m->filtered_count = 0;
    if (m->font_query[0] == '\0' || str_icontains("System Default 系统默认 默认字体 Segoe UI", m->font_query)) {
        m->filtered_idx[m->filtered_count++] = -1;
    }
    if (fonts) {
        for (int i = 0; i < fonts->count; ++i) {
            if (str_icontains(fonts->names[i], m->font_query)) {
                m->filtered_idx[m->filtered_count++] = i;
            }
        }
    }
    m->font_scroll_y = 0.0f;
}

void ks_settings_open(KsSettingsModal *m, const KsConfig *cfg, const char *data_dir_utf8,
                      const KsFontList *fonts, HWND parent_hwnd) {
    (void)parent_hwnd;
    m->open = true;
    m->confirm_clear_open = false;
    m->draft = *cfg;
    m->original = *cfg;
    snprintf(m->data_dir, sizeof(m->data_dir), "%s", data_dir_utf8 ? data_dir_utf8 : "");
    m->open_drop = KS_SETTINGS_DROP_NONE;
    m->font_query[0] = '\0';
    m->font_scroll_y = 0.0f;
    m->hover_drop_item = -1;
    m->hover_control = 0;
    destroy_search_edit(m);
    ks_settings_filter_fonts(m, fonts, "");
}

void ks_settings_close(KsSettingsModal *m, KsD2DRenderer *r, bool apply_changes) {
    if (!m) return;
    destroy_search_edit(m);
    m->open = false;
    m->confirm_clear_open = false;
    m->open_drop = KS_SETTINGS_DROP_NONE;
    if (!apply_changes && r) {
        ks_d2d_set_font_family(r, m->original.font_family);
    }
}

void ks_settings_on_search_changed(KsSettingsModal *m, const KsFontList *fonts) {
    if (!m || !m->search_edit) return;
    wchar_t wbuf[128];
    GetWindowTextW(m->search_edit, wbuf, 128);
    char u8[256];
    if (!ks_wide_to_utf8_buf(wbuf, u8, sizeof(u8))) u8[0] = '\0';
    ks_settings_filter_fonts(m, fonts, u8);
}

typedef struct {
    KsRect close_btn;
    KsRect row_rc[8];
    KsRect ctl_lang;
    KsRect ctl_theme;
    KsRect ctl_scheme;
    KsRect ctl_font;
    KsRect ctl_start_min;
    KsRect ctl_auto_start;
    KsRect ctl_open_dir;
    KsRect ctl_clear_stats;
    KsRect cancel_btn;
    KsRect save_btn;
    float header_bottom;
    float footer_top;
} KsSettingsLayout;

static void compute_layout(KsRect modal, KsSettingsLayout *out) {
    float pad = 24.0f;
    float cw = modal.w - pad * 2.0f;

    out->close_btn = (KsRect){ modal.x + modal.w - pad - 28.0f, modal.y + 20.0f, 28.0f, 28.0f };
    out->header_bottom = modal.y + 68.0f;
    out->footer_top = modal.y + modal.h - 58.0f;

    float body_top = out->header_bottom + 4.0f;
    float body_h = out->footer_top - body_top - 4.0f;
    float row_h = body_h / 8.0f;
    if (row_h > 52.0f) row_h = 52.0f;

    for (int i = 0; i < 8; ++i) {
        out->row_rc[i] = (KsRect){ modal.x + pad, body_top + (float)i * row_h, cw, row_h };
    }

    float drop_w = (cw > 420.0f) ? 188.0f : 156.0f;
    float drop_h = 32.0f;

    out->ctl_lang = (KsRect){ out->row_rc[0].x + cw - drop_w,
                              out->row_rc[0].y + (row_h - drop_h) * 0.5f, drop_w, drop_h };
    out->ctl_theme = (KsRect){ out->row_rc[1].x + cw - drop_w,
                               out->row_rc[1].y + (row_h - drop_h) * 0.5f, drop_w, drop_h };
    out->ctl_scheme = (KsRect){ out->row_rc[2].x + cw - drop_w,
                                out->row_rc[2].y + (row_h - drop_h) * 0.5f, drop_w, drop_h };
    out->ctl_font = (KsRect){ out->row_rc[3].x + cw - 208.0f,
                              out->row_rc[3].y + (row_h - drop_h) * 0.5f, 208.0f, drop_h };

    out->ctl_start_min = out->row_rc[4];
    out->ctl_auto_start = out->row_rc[5];

    float open_w = 104.0f;
    out->ctl_open_dir = (KsRect){ out->row_rc[6].x + cw - open_w,
                                  out->row_rc[6].y + (row_h - 30.0f) * 0.5f, open_w, 30.0f };

    float clear_w = 104.0f;
    out->ctl_clear_stats = (KsRect){ out->row_rc[7].x + cw - clear_w,
                                     out->row_rc[7].y + (row_h - 30.0f) * 0.5f, clear_w, 30.0f };

    float btn_w = 96.0f;
    float btn_h = 34.0f;
    float btn_y = out->footer_top + (58.0f - btn_h) * 0.5f;
    out->save_btn = (KsRect){ modal.x + modal.w - pad - btn_w, btn_y, btn_w, btn_h };
    out->cancel_btn = (KsRect){ out->save_btn.x - 10.0f - btn_w, btn_y, btn_w, btn_h };
}

static KsRect compute_font_dropdown_rect(KsRect font_btn, float win_h) {
    float drop_w = 264.0f;
    float drop_h = 224.0f;
    float x = font_btn.x + font_btn.w - drop_w;
    float y = font_btn.y + font_btn.h + 6.0f;
    if (y + drop_h > win_h - 14.0f) {
        y = font_btn.y - drop_h - 6.0f;
        if (y < 14.0f) y = 14.0f;
    }
    return (KsRect){ x, y, drop_w, drop_h };
}

static void ensure_search_edit(KsSettingsModal *m, HWND parent_hwnd, const KsThemePalette *pal,
                               KsRect search_rect, float dpi) {
    m->search_box_rect = search_rect;
    float scale = dpi / 96.0f;
    int px_x = (int)((search_rect.x + 10.0f) * scale);
    int px_y = (int)((search_rect.y + 8.0f) * scale);
    int px_w = (int)((search_rect.w - 20.0f) * scale);
    int px_h = (int)((search_rect.h - 16.0f) * scale);
    if (px_w < 20) px_w = 20;
    if (px_h < 14) px_h = 14;

    if (!m->search_edit && parent_hwnd) {
        m->search_edit = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
            px_x, px_y, px_w, px_h,
            parent_hwnd, (HMENU)(INT_PTR)2001, GetModuleHandleW(NULL), NULL);
        if (m->search_edit) {
            int font_px = (int)(-13.0f * scale);
            m->search_hfont = CreateFontW(font_px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            if (m->search_hfont) {
                SendMessageW(m->search_edit, WM_SETFONT, (WPARAM)m->search_hfont, TRUE);
            }
            SetFocus(m->search_edit);
        }
    } else if (m->search_edit) {
        MoveWindow(m->search_edit, px_x, px_y, px_w, px_h, TRUE);
    }

    if (pal) {
        KsColor bg = pal->surface_raised;
        COLORREF cr = RGB((BYTE)(bg.r * 255.0f), (BYTE)(bg.g * 255.0f), (BYTE)(bg.b * 255.0f));
        if (m->search_bg_brush) DeleteObject(m->search_bg_brush);
        m->search_bg_brush = CreateSolidBrush(cr);
    }
}

void ks_settings_sync_edit_theme(KsSettingsModal *m, const KsThemePalette *pal, float dpi) {
    if (!m || !m->open || m->open_drop != KS_SETTINGS_DROP_FONT || !m->search_edit) return;
    ensure_search_edit(m, NULL, pal, m->search_box_rect, dpi);
    InvalidateRect(m->search_edit, NULL, TRUE);
}

HBRUSH ks_settings_ctlcolor_edit(KsSettingsModal *m, HDC hdc, const KsThemePalette *pal) {
    if (!m || !pal) return NULL;
    SetTextColor(hdc, RGB((BYTE)(pal->text_primary.r * 255.0f),
                          (BYTE)(pal->text_primary.g * 255.0f),
                          (BYTE)(pal->text_primary.b * 255.0f)));
    KsColor bg = pal->surface_raised;
    COLORREF cr = RGB((BYTE)(bg.r * 255.0f), (BYTE)(bg.g * 255.0f), (BYTE)(bg.b * 255.0f));
    SetBkColor(hdc, cr);
    if (!m->search_bg_brush) m->search_bg_brush = CreateSolidBrush(cr);
    return m->search_bg_brush;
}

bool ks_settings_handle_wheel(KsSettingsModal *m, float mx, float my, float win_w, float win_h, int delta) {
    if (!m || !m->open || m->confirm_clear_open || m->open_drop != KS_SETTINGS_DROP_FONT) return false;
    KsRect modal = compute_modal_rect(win_w, win_h);
    KsSettingsLayout lay;
    compute_layout(modal, &lay);
    KsRect drop = compute_font_dropdown_rect(lay.ctl_font, win_h);
    if (!ks_rect_contains(drop, mx, my)) return false;

    float list_h = drop.h - 54.0f;
    float item_h = 34.0f;
    float max_scroll = (float)m->filtered_count * item_h - list_h;
    if (max_scroll < 0.0f) max_scroll = 0.0f;

    m->font_scroll_y -= ((float)delta / 120.0f) * 54.0f;
    if (m->font_scroll_y < 0.0f) m->font_scroll_y = 0.0f;
    if (m->font_scroll_y > max_scroll) m->font_scroll_y = max_scroll;
    return true;
}

static const char *lang_display_label(KsLang ui_lang, const char *pref) {
    if (_stricmp(pref, "auto") == 0 || pref[0] == '\0') return ks_tr(ui_lang, KS_STR_LANG_AUTO);
    if (_strnicmp(pref, "en", 2) == 0) return "English";
    return "简体中文";
}

static const char *theme_label(KsLang lang, const char *pref) {
    if (_stricmp(pref, "dark") == 0) return ks_tr(lang, KS_STR_THEME_DARK);
    if (_stricmp(pref, "light") == 0) return ks_tr(lang, KS_STR_THEME_LIGHT);
    return ks_tr(lang, KS_STR_THEME_AUTO);
}

static KsStringId scheme_string_id(KsColorScheme s) {
    switch (s) {
    case KS_SCHEME_OCEAN: return KS_STR_SCHEME_OCEAN;
    case KS_SCHEME_EMERALD: return KS_STR_SCHEME_EMERALD;
    case KS_SCHEME_AMBER: return KS_STR_SCHEME_AMBER;
    case KS_SCHEME_ROSE: return KS_STR_SCHEME_ROSE;
    case KS_SCHEME_INDIGO:
    default: return KS_STR_SCHEME_INDIGO;
    }
}

static KsRect compute_simple_drop_rect(KsRect ctl, int count, float win_h) {
    float h = 12.0f + (float)count * 34.0f;
    float y = ctl.y + ctl.h + 6.0f;
    if (y + h > win_h - 14.0f) {
        y = ctl.y - h - 6.0f;
        if (y < 14.0f) y = 14.0f;
    }
    return (KsRect){ ctl.x, y, ctl.w, h };
}

int ks_settings_handle_click(KsSettingsModal *m, KsD2DRenderer *r, const KsFontList *fonts,
                             HWND parent_hwnd, float win_w, float win_h, float mx, float my) {
    if (!m || !m->open) return 0;

    if (m->confirm_clear_open) {
        KsRect dlg = compute_confirm_rect(win_w, win_h);
        float btn_w = 112.0f;
        KsRect confirm_btn = { dlg.x + dlg.w - 24.0f - btn_w, dlg.y + dlg.h - 20.0f - 34.0f, btn_w, 34.0f };
        KsRect cancel_btn = { confirm_btn.x - 10.0f - btn_w, confirm_btn.y, btn_w, 34.0f };
        if (!ks_rect_contains(dlg, mx, my) || ks_rect_contains(cancel_btn, mx, my)) {
            m->confirm_clear_open = false;
            return 1;
        }
        if (ks_rect_contains(confirm_btn, mx, my)) {
            m->confirm_clear_open = false;
            return 4;
        }
        return 1;
    }

    KsRect modal = compute_modal_rect(win_w, win_h);
    KsSettingsLayout lay;
    compute_layout(modal, &lay);

    if (m->open_drop == KS_SETTINGS_DROP_LANG) {
        KsRect drop = compute_simple_drop_rect(lay.ctl_lang, 3, win_h);
        if (ks_rect_contains(drop, mx, my)) {
            int idx = (int)((my - (drop.y + 6.0f)) / 34.0f);
            const char *vals[3] = { "auto", "zh", "en" };
            if (idx >= 0 && idx < 3) {
                snprintf(m->draft.language, sizeof(m->draft.language), "%s", vals[idx]);
            }
        }
        m->open_drop = KS_SETTINGS_DROP_NONE;
        return 1;
    }

    if (m->open_drop == KS_SETTINGS_DROP_THEME) {
        KsRect drop = compute_simple_drop_rect(lay.ctl_theme, 3, win_h);
        if (ks_rect_contains(drop, mx, my)) {
            int idx = (int)((my - (drop.y + 6.0f)) / 34.0f);
            const char *vals[3] = { "auto", "dark", "light" };
            if (idx >= 0 && idx < 3) {
                snprintf(m->draft.theme, sizeof(m->draft.theme), "%s", vals[idx]);
            }
        }
        m->open_drop = KS_SETTINGS_DROP_NONE;
        return 1;
    }

    if (m->open_drop == KS_SETTINGS_DROP_SCHEME) {
        KsRect drop = compute_simple_drop_rect(lay.ctl_scheme, KS_SCHEME_COUNT, win_h);
        if (ks_rect_contains(drop, mx, my)) {
            int idx = (int)((my - (drop.y + 6.0f)) / 34.0f);
            if (idx >= 0 && idx < KS_SCHEME_COUNT) {
                snprintf(m->draft.color_scheme, sizeof(m->draft.color_scheme), "%s",
                         ks_scheme_to_code((KsColorScheme)idx));
            }
        }
        m->open_drop = KS_SETTINGS_DROP_NONE;
        return 1;
    }

    if (m->open_drop == KS_SETTINGS_DROP_FONT) {
        KsRect drop = compute_font_dropdown_rect(lay.ctl_font, win_h);
        if (ks_rect_contains(drop, mx, my)) {
            KsRect search_rc = { drop.x + 10.0f, drop.y + 10.0f, drop.w - 20.0f, 34.0f };
            if (ks_rect_contains(search_rc, mx, my)) {
                if (m->search_edit) SetFocus(m->search_edit);
                return 1;
            }
            float list_y = drop.y + 50.0f;
            float list_h = drop.h - 56.0f;
            if (my >= list_y && my <= list_y + list_h) {
                int rel = (int)((my - list_y + m->font_scroll_y) / 34.0f);
                if (rel >= 0 && rel < m->filtered_count) {
                    int fi = m->filtered_idx[rel];
                    if (fi < 0) {
                        m->draft.font_family[0] = '\0';
                    } else if (fonts && fi < fonts->count) {
                        snprintf(m->draft.font_family, sizeof(m->draft.font_family), "%s", fonts->names[fi]);
                    }
                    if (r) ks_d2d_set_font_family(r, m->draft.font_family);
                    destroy_search_edit(m);
                    m->open_drop = KS_SETTINGS_DROP_NONE;
                }
                return 1;
            }
            return 1;
        }
        destroy_search_edit(m);
        m->open_drop = KS_SETTINGS_DROP_NONE;
        return 1;
    }

    if (!ks_rect_contains(modal, mx, my) || ks_rect_contains(lay.close_btn, mx, my) ||
        ks_rect_contains(lay.cancel_btn, mx, my)) {
        ks_settings_close(m, r, false);
        return 3;
    }
    if (ks_rect_contains(lay.save_btn, mx, my)) {
        ks_settings_close(m, r, true);
        return 2;
    }
    if (ks_rect_contains(lay.ctl_lang, mx, my)) {
        m->open_drop = KS_SETTINGS_DROP_LANG;
        return 1;
    }
    if (ks_rect_contains(lay.ctl_theme, mx, my)) {
        m->open_drop = KS_SETTINGS_DROP_THEME;
        return 1;
    }
    if (ks_rect_contains(lay.ctl_scheme, mx, my)) {
        m->open_drop = KS_SETTINGS_DROP_SCHEME;
        return 1;
    }
    if (ks_rect_contains(lay.ctl_font, mx, my)) {
        m->open_drop = KS_SETTINGS_DROP_FONT;
        ks_settings_filter_fonts(m, fonts, "");
        KsRect drop = compute_font_dropdown_rect(lay.ctl_font, win_h);
        KsRect search_rc = { drop.x + 10.0f, drop.y + 10.0f, drop.w - 20.0f, 34.0f };
        ensure_search_edit(m, parent_hwnd, NULL, search_rc,
                           parent_hwnd ? (float)GetDpiForWindow(parent_hwnd) : 96.0f);
        return 1;
    }
    if (ks_rect_contains(lay.ctl_start_min, mx, my)) {
        m->draft.start_minimized = !m->draft.start_minimized;
        return 1;
    }
    if (ks_rect_contains(lay.ctl_auto_start, mx, my)) {
        m->draft.auto_start = !m->draft.auto_start;
        return 1;
    }
    if (ks_rect_contains(lay.ctl_open_dir, mx, my) && m->data_dir[0] != '\0') {
        wchar_t *wdir = ks_utf8_to_wide_alloc(m->data_dir);
        if (wdir) {
            ShellExecuteW(NULL, L"open", wdir, NULL, NULL, SW_SHOWNORMAL);
            free(wdir);
        }
        return 1;
    }
    if (ks_rect_contains(lay.ctl_clear_stats, mx, my)) {
        m->confirm_clear_open = true;
        return 1;
    }
    return 1;
}

static void draw_row_label(KsD2DRenderer *r, const KsThemePalette *pal, KsRect row, float right_reserved,
                           const char *title, const char *desc) {
    float text_w = row.w - right_reserved - 20.0f;
    float mid = row.y + row.h * 0.5f;
    ks_d2d_draw_text(r, title, row.x + 2.0f, mid - 18.0f, text_w, 19.0f,
                     13.0f, KS_FONT_SEMIBOLD, pal->text_primary, KS_ALIGN_LEFT, true, true, NULL);
    ks_d2d_draw_text(r, desc, row.x + 2.0f, mid + 1.0f, text_w, 16.0f,
                     11.5f, KS_FONT_REGULAR, pal->text_tertiary, KS_ALIGN_LEFT, true, true, NULL);
}

static void draw_select_pill(KsD2DRenderer *r, const KsThemePalette *pal, KsRect rc,
                             const char *text, const char *font_override,
                             bool has_swatch, KsColor swatch_col, bool hovered, bool active) {
    KsColor bg = (hovered || active) ? ks_color_lerp(pal->surface_overlay, pal->text_primary, 0.06f)
                                     : pal->surface_overlay;
    ks_d2d_fill_rounded_rect(r, rc.x, rc.y, rc.w, rc.h, 9.0f, bg);
    float text_x = rc.x + (has_swatch ? 28.0f : 12.0f);
    if (has_swatch) {
        ks_d2d_fill_circle(r, rc.x + 15.0f, rc.y + rc.h * 0.5f, 4.5f, swatch_col);
    }
    ks_d2d_draw_text(r, text, text_x, rc.y, rc.w - (text_x - rc.x) - 24.0f, rc.h,
                     12.5f, KS_FONT_MEDIUM, active ? pal->accent : pal->text_primary,
                     KS_ALIGN_LEFT, true, true, font_override);
    ks_d2d_draw_chevron_down(r, rc.x + rc.w - 15.0f, rc.y + rc.h * 0.5f, 9.5f,
                             active ? pal->accent : pal->text_secondary, active);
}

void ks_settings_draw(KsSettingsModal *m, KsD2DRenderer *r, const KsThemePalette *pal,
                      KsLang lang, const KsFontList *fonts,
                      float win_w, float win_h, float mouse_x, float mouse_y, float dpi) {
    if (!m || !m->open) return;

    ks_draw_modal_scrim(r, win_w, win_h, 1.0f);

    KsRect modal = compute_modal_rect(win_w, win_h);
    ks_d2d_draw_glass_panel(r, modal.x, modal.y, modal.w, modal.h, 18.0f,
                            18.0f, pal->glass_tint, pal->glass_border, pal->shadow_color);

    KsSettingsLayout lay;
    compute_layout(modal, &lay);
    float pad = 24.0f;

    /* Modal Header */
    ks_d2d_draw_text(r, ks_tr(lang, KS_STR_SETTINGS_TITLE),
                     modal.x + pad, modal.y + 18.0f, modal.w - 84.0f, 22.0f,
                     17.0f, KS_FONT_BOLD, pal->text_primary, KS_ALIGN_LEFT, true, true, NULL);
    ks_d2d_draw_text(r, ks_tr(lang, KS_STR_SETTINGS_SUBTITLE),
                     modal.x + pad, modal.y + 42.0f, modal.w - 84.0f, 18.0f,
                     11.5f, KS_FONT_REGULAR, pal->text_secondary, KS_ALIGN_LEFT, true, true, NULL);

    bool base_interactive = (m->open_drop == KS_SETTINGS_DROP_NONE && !m->confirm_clear_open);
    if (base_interactive && ks_rect_contains(lay.close_btn, mouse_x, mouse_y)) {
        ks_d2d_fill_rounded_rect(r, lay.close_btn.x, lay.close_btn.y, lay.close_btn.w, lay.close_btn.h,
                                 8.0f, pal->surface_overlay);
    }
    ks_d2d_draw_close_x(r, lay.close_btn.x + lay.close_btn.w * 0.5f,
                        lay.close_btn.y + lay.close_btn.h * 0.5f, 12.0f, pal->text_secondary);

    /* Row 0: Language */
    draw_row_label(r, pal, lay.row_rc[0], lay.ctl_lang.w,
                   ks_tr(lang, KS_STR_SETTINGS_LANGUAGE), ks_tr(lang, KS_STR_SETTINGS_LANGUAGE_DESC));
    draw_select_pill(r, pal, lay.ctl_lang, lang_display_label(lang, m->draft.language), NULL,
                     false, pal->accent,
                     base_interactive && ks_rect_contains(lay.ctl_lang, mouse_x, mouse_y),
                     m->open_drop == KS_SETTINGS_DROP_LANG);

    /* Row 1: Theme */
    draw_row_label(r, pal, lay.row_rc[1], lay.ctl_theme.w,
                   ks_tr(lang, KS_STR_SETTINGS_THEME), ks_tr(lang, KS_STR_SETTINGS_THEME_DESC));
    draw_select_pill(r, pal, lay.ctl_theme, theme_label(lang, m->draft.theme), NULL,
                     false, pal->accent,
                     base_interactive && ks_rect_contains(lay.ctl_theme, mouse_x, mouse_y),
                     m->open_drop == KS_SETTINGS_DROP_THEME);

    /* Row 2: Color Scheme */
    KsColorScheme draft_scheme = ks_scheme_from_code(m->draft.color_scheme);
    draw_row_label(r, pal, lay.row_rc[2], lay.ctl_scheme.w,
                   ks_tr(lang, KS_STR_SETTINGS_SCHEME), ks_tr(lang, KS_STR_SETTINGS_SCHEME_DESC));
    draw_select_pill(r, pal, lay.ctl_scheme, ks_tr(lang, scheme_string_id(draft_scheme)), NULL,
                     true, ks_scheme_swatch(draft_scheme, pal->dark_mode),
                     base_interactive && ks_rect_contains(lay.ctl_scheme, mouse_x, mouse_y),
                     m->open_drop == KS_SETTINGS_DROP_SCHEME);

    /* Row 3: Font Family */
    draw_row_label(r, pal, lay.row_rc[3], lay.ctl_font.w,
                   ks_tr(lang, KS_STR_SETTINGS_FONT), ks_tr(lang, KS_STR_SETTINGS_FONT_DESC));
    const char *font_disp = m->draft.font_family[0] ? m->draft.font_family
                                                   : ks_tr(lang, KS_STR_SETTINGS_FONT_DEFAULT);
    draw_select_pill(r, pal, lay.ctl_font, font_disp,
                     m->draft.font_family[0] ? m->draft.font_family : NULL,
                     false, pal->accent,
                     base_interactive && ks_rect_contains(lay.ctl_font, mouse_x, mouse_y),
                     m->open_drop == KS_SETTINGS_DROP_FONT);

    /* Row 4: Start Minimized */
    bool h_sm = base_interactive && ks_rect_contains(lay.ctl_start_min, mouse_x, mouse_y);
    draw_row_label(r, pal, lay.row_rc[4], 56.0f,
                   ks_tr(lang, KS_STR_SETTINGS_START_MINIMIZED),
                   ks_tr(lang, KS_STR_SETTINGS_START_MINIMIZED_DESC));
    ks_draw_toggle_switch(r, pal, lay.row_rc[4].x + lay.row_rc[4].w - 40.0f,
                          lay.row_rc[4].y + (lay.row_rc[4].h - 22.0f) * 0.5f,
                          m->draft.start_minimized, h_sm);

    /* Row 5: Auto Start */
    bool h_as = base_interactive && ks_rect_contains(lay.ctl_auto_start, mouse_x, mouse_y);
    draw_row_label(r, pal, lay.row_rc[5], 56.0f,
                   ks_tr(lang, KS_STR_SETTINGS_AUTO_START),
                   ks_tr(lang, KS_STR_SETTINGS_AUTO_START_DESC));
    ks_draw_toggle_switch(r, pal, lay.row_rc[5].x + lay.row_rc[5].w - 40.0f,
                          lay.row_rc[5].y + (lay.row_rc[5].h - 22.0f) * 0.5f,
                          m->draft.auto_start, h_as);

    /* Row 6: Data Directory (+ inline Portable marker if active) */
    char dir_desc[MAX_PATH * 3 + 32];
    if (m->draft.portable_mode) {
        snprintf(dir_desc, sizeof(dir_desc), "[%s] %s", ks_tr(lang, KS_STR_SETTINGS_PORTABLE_TAG), m->data_dir);
    } else {
        snprintf(dir_desc, sizeof(dir_desc), "%s", m->data_dir);
    }
    draw_row_label(r, pal, lay.row_rc[6], lay.ctl_open_dir.w,
                   ks_tr(lang, KS_STR_SETTINGS_DATA_DIR), dir_desc);
    bool h_dir = base_interactive && ks_rect_contains(lay.ctl_open_dir, mouse_x, mouse_y);
    ks_draw_button(r, pal, lay.ctl_open_dir, ks_tr(lang, KS_STR_SETTINGS_OPEN_FOLDER),
                   KS_BTN_SECONDARY, h_dir, false, 12.0f, 8.0f);

    /* Row 7: Clear Statistics (Step 1 Danger Button) */
    draw_row_label(r, pal, lay.row_rc[7], lay.ctl_clear_stats.w,
                   ks_tr(lang, KS_STR_SETTINGS_CLEAR_STATS), ks_tr(lang, KS_STR_SETTINGS_CLEAR_STATS_DESC));
    bool h_clr = base_interactive && ks_rect_contains(lay.ctl_clear_stats, mouse_x, mouse_y);
    ks_draw_button(r, pal, lay.ctl_clear_stats, ks_tr(lang, KS_STR_SETTINGS_CLEAR_BTN),
                   KS_BTN_DANGER, h_clr, false, 12.0f, 8.0f);

    /* Modal Footer */
    bool h_cancel = base_interactive && ks_rect_contains(lay.cancel_btn, mouse_x, mouse_y);
    bool h_save = base_interactive && ks_rect_contains(lay.save_btn, mouse_x, mouse_y);
    ks_draw_button(r, pal, lay.cancel_btn, ks_tr(lang, KS_STR_BTN_CANCEL), KS_BTN_SECONDARY, h_cancel, false, 12.5f, 9.0f);
    ks_draw_button(r, pal, lay.save_btn, ks_tr(lang, KS_STR_BTN_SAVE), KS_BTN_PRIMARY, h_save, false, 12.5f, 9.0f);

    /* Active Glass Dropdown Overlays */
    if (m->open_drop == KS_SETTINGS_DROP_LANG) {
        KsRect drop = compute_simple_drop_rect(lay.ctl_lang, 3, win_h);
        ks_d2d_draw_glass_panel(r, drop.x, drop.y, drop.w, drop.h, 12.0f, 14.0f,
                                pal->glass_tint, pal->glass_border, pal->shadow_color);
        const char *opts[3] = { ks_tr(lang, KS_STR_LANG_AUTO), "简体中文", "English" };
        bool sel[3] = {
            (_stricmp(m->draft.language, "auto") == 0 || m->draft.language[0] == '\0'),
            (_strnicmp(m->draft.language, "zh", 2) == 0),
            (_strnicmp(m->draft.language, "en", 2) == 0)
        };
        for (int i = 0; i < 3; ++i) {
            KsRect ir = { drop.x + 6.0f, drop.y + 6.0f + (float)i * 34.0f, drop.w - 12.0f, 32.0f };
            bool hov = ks_rect_contains(ir, mouse_x, mouse_y);
            if (hov || sel[i]) {
                ks_d2d_fill_rounded_rect(r, ir.x, ir.y, ir.w, ir.h, 7.0f,
                                         sel[i] ? ks_color_alpha(pal->accent, 0.16f)
                                                : pal->surface_overlay);
            }
            ks_d2d_draw_text(r, opts[i], ir.x + 10.0f, ir.y, ir.w - 30.0f, ir.h,
                             12.5f, sel[i] ? KS_FONT_SEMIBOLD : KS_FONT_REGULAR,
                             sel[i] ? pal->accent : pal->text_primary, KS_ALIGN_LEFT, true, true, NULL);
            if (sel[i]) {
                ks_d2d_draw_checkmark(r, ir.x + ir.w - 14.0f, ir.y + ir.h * 0.5f, 11.0f, pal->accent);
            }
        }
    } else if (m->open_drop == KS_SETTINGS_DROP_THEME) {
        KsRect drop = compute_simple_drop_rect(lay.ctl_theme, 3, win_h);
        ks_d2d_draw_glass_panel(r, drop.x, drop.y, drop.w, drop.h, 12.0f, 14.0f,
                                pal->glass_tint, pal->glass_border, pal->shadow_color);
        const char *codes[3] = { "auto", "dark", "light" };
        KsStringId sids[3] = { KS_STR_THEME_AUTO, KS_STR_THEME_DARK, KS_STR_THEME_LIGHT };
        for (int i = 0; i < 3; ++i) {
            KsRect ir = { drop.x + 6.0f, drop.y + 6.0f + (float)i * 34.0f, drop.w - 12.0f, 32.0f };
            bool sel = (_stricmp(m->draft.theme, codes[i]) == 0);
            bool hov = ks_rect_contains(ir, mouse_x, mouse_y);
            if (hov || sel) {
                ks_d2d_fill_rounded_rect(r, ir.x, ir.y, ir.w, ir.h, 7.0f,
                                         sel ? ks_color_alpha(pal->accent, 0.16f)
                                                : pal->surface_overlay);
            }
            ks_d2d_draw_text(r, ks_tr(lang, sids[i]), ir.x + 10.0f, ir.y, ir.w - 30.0f, ir.h,
                             12.5f, sel ? KS_FONT_SEMIBOLD : KS_FONT_REGULAR,
                             sel ? pal->accent : pal->text_primary, KS_ALIGN_LEFT, true, true, NULL);
            if (sel) {
                ks_d2d_draw_checkmark(r, ir.x + ir.w - 14.0f, ir.y + ir.h * 0.5f, 11.0f, pal->accent);
            }
        }
    } else if (m->open_drop == KS_SETTINGS_DROP_SCHEME) {
        KsRect drop = compute_simple_drop_rect(lay.ctl_scheme, KS_SCHEME_COUNT, win_h);
        ks_d2d_draw_glass_panel(r, drop.x, drop.y, drop.w, drop.h, 12.0f, 14.0f,
                                pal->glass_tint, pal->glass_border, pal->shadow_color);
        for (int i = 0; i < KS_SCHEME_COUNT; ++i) {
            KsColorScheme sc = (KsColorScheme)i;
            KsRect ir = { drop.x + 6.0f, drop.y + 6.0f + (float)i * 34.0f, drop.w - 12.0f, 32.0f };
            bool sel = (draft_scheme == sc);
            bool hov = ks_rect_contains(ir, mouse_x, mouse_y);
            if (hov || sel) {
                ks_d2d_fill_rounded_rect(r, ir.x, ir.y, ir.w, ir.h, 7.0f,
                                         sel ? ks_color_alpha(pal->accent, 0.16f)
                                             : pal->surface_overlay);
            }
            ks_d2d_fill_circle(r, ir.x + 14.0f, ir.y + ir.h * 0.5f, 4.5f, ks_scheme_swatch(sc, pal->dark_mode));
            ks_d2d_draw_text(r, ks_tr(lang, scheme_string_id(sc)), ir.x + 26.0f, ir.y, ir.w - 46.0f, ir.h,
                             12.5f, sel ? KS_FONT_SEMIBOLD : KS_FONT_REGULAR,
                             sel ? pal->accent : pal->text_primary, KS_ALIGN_LEFT, true, true, NULL);
            if (sel) {
                ks_d2d_draw_checkmark(r, ir.x + ir.w - 14.0f, ir.y + ir.h * 0.5f, 11.0f, pal->accent);
            }
        }
    } else if (m->open_drop == KS_SETTINGS_DROP_FONT) {
        KsRect drop = compute_font_dropdown_rect(lay.ctl_font, win_h);
        ks_d2d_draw_glass_panel(r, drop.x, drop.y, drop.w, drop.h, 14.0f, 16.0f,
                                pal->glass_tint, pal->glass_border, pal->shadow_color);

        KsRect search_rc = { drop.x + 10.0f, drop.y + 10.0f, drop.w - 20.0f, 34.0f };
        ks_d2d_fill_rounded_rect(r, search_rc.x, search_rc.y, search_rc.w, search_rc.h, 8.0f, pal->surface_raised);
        if (m->font_query[0] == '\0' && (!m->search_edit || GetWindowTextLengthW(m->search_edit) == 0)) {
            ks_d2d_draw_text(r, ks_tr(lang, KS_STR_SETTINGS_FONT_SEARCH),
                             search_rc.x + 10.0f, search_rc.y, search_rc.w - 20.0f, search_rc.h,
                             12.0f, KS_FONT_REGULAR, pal->text_tertiary, KS_ALIGN_LEFT, true, true, NULL);
        }
        ensure_search_edit(m, NULL, pal, search_rc, dpi);

        /* Virtualized list viewport */
        float list_x = drop.x + 8.0f;
        float list_y = drop.y + 50.0f;
        float list_w = drop.w - 16.0f;
        float list_h = drop.h - 56.0f;
        float item_h = 34.0f;

        ks_d2d_push_clip(r, list_x, list_y, list_w, list_h, 0.0f);
        if (m->filtered_count == 0) {
            ks_d2d_draw_text(r, ks_tr(lang, KS_STR_SETTINGS_FONT_NO_MATCH),
                             list_x, list_y, list_w, 48.0f,
                             12.5f, KS_FONT_REGULAR, pal->text_tertiary, KS_ALIGN_CENTER, true, true, NULL);
        } else {
            int first = 0, last = -1;
            ks_settings_visible_range(m->filtered_count, m->font_scroll_y, list_h, item_h, &first, &last);
            for (int row = first; row <= last; ++row) {
                int fi = m->filtered_idx[row];
                const char *fname = (fi < 0) ? ks_tr(lang, KS_STR_SETTINGS_FONT_DEFAULT)
                                             : (fonts ? fonts->names[fi] : "");
                const char *preview_face = (fi < 0) ? NULL : fname;
                bool sel = (fi < 0) ? (m->draft.font_family[0] == '\0')
                                    : (_stricmp(m->draft.font_family, fname) == 0);
                float iy = list_y + (float)row * item_h - m->font_scroll_y;
                KsRect ir = { list_x + 2.0f, iy + 1.0f, list_w - 10.0f, item_h - 2.0f };
                bool hov = (mouse_y >= list_y && mouse_y <= list_y + list_h) && ks_rect_contains(ir, mouse_x, mouse_y);
                if (sel || hov) {
                    ks_d2d_fill_rounded_rect(r, ir.x, ir.y, ir.w, ir.h, 7.0f,
                                             sel ? ks_color_alpha(pal->accent, 0.16f)
                                                 : pal->surface_overlay);
                }
                ks_d2d_draw_text(r, fname, ir.x + 10.0f, ir.y, ir.w - 34.0f, ir.h,
                                 12.5f, sel ? KS_FONT_SEMIBOLD : KS_FONT_REGULAR,
                                 sel ? pal->accent : pal->text_primary,
                                 KS_ALIGN_LEFT, true, true, preview_face);
                if (sel) {
                    ks_d2d_draw_checkmark(r, ir.x + ir.w - 14.0f, ir.y + ir.h * 0.5f, 11.0f, pal->accent);
                }
            }
        }
        ks_d2d_pop_clip(r);

        float content_h = (float)m->filtered_count * item_h;
        if (content_h > list_h) {
            float bar_h = list_h * (list_h / content_h);
            if (bar_h < 22.0f) bar_h = 22.0f;
            float max_scroll = content_h - list_h;
            float thumb_y = list_y + (list_h - bar_h) * (m->font_scroll_y / max_scroll);
            ks_d2d_fill_rounded_rect(r, list_x + list_w - 5.0f, thumb_y, 3.5f, bar_h, 2.0f, pal->text_tertiary);
        }
    }

    /* Step 2 Red Confirmation Modal over Settings */
    if (m->confirm_clear_open) {
        ks_draw_modal_scrim(r, win_w, win_h, 0.9f);
        KsRect dlg = compute_confirm_rect(win_w, win_h);
        ks_d2d_draw_glass_panel(r, dlg.x, dlg.y, dlg.w, dlg.h, 18.0f, 18.0f,
                                pal->glass_tint, pal->glass_border, pal->shadow_color);

        /* Soft danger tint wash at top of confirmation dialog */
        ks_d2d_fill_rounded_rect(r, dlg.x, dlg.y, dlg.w, 56.0f, 18.0f, ks_color_alpha(pal->danger, 0.14f));
        ks_d2d_fill_circle(r, dlg.x + 30.0f, dlg.y + 32.0f, 5.0f, pal->danger);
        ks_d2d_draw_text(r, ks_tr(lang, KS_STR_RESET_TITLE),
                         dlg.x + 44.0f, dlg.y + 18.0f, dlg.w - 68.0f, 28.0f,
                         16.0f, KS_FONT_BOLD, pal->danger, KS_ALIGN_LEFT, true, true, NULL);

        ks_d2d_draw_text(r, ks_tr(lang, KS_STR_RESET_CONFIRM_MSG),
                         dlg.x + 24.0f, dlg.y + 68.0f, dlg.w - 48.0f, 68.0f,
                         12.5f, KS_FONT_REGULAR, pal->text_secondary, KS_ALIGN_LEFT, true, false, NULL);

        float btn_w = 112.0f;
        KsRect confirm_btn = { dlg.x + dlg.w - 24.0f - btn_w, dlg.y + dlg.h - 20.0f - 34.0f, btn_w, 34.0f };
        KsRect cancel_btn = { confirm_btn.x - 10.0f - btn_w, confirm_btn.y, btn_w, 34.0f };
        ks_draw_button(r, pal, cancel_btn, ks_tr(lang, KS_STR_BTN_CANCEL), KS_BTN_SECONDARY,
                       ks_rect_contains(cancel_btn, mouse_x, mouse_y), false, 12.5f, 9.0f);
        ks_draw_button(r, pal, confirm_btn, ks_tr(lang, KS_STR_BTN_CONFIRM_RESET), KS_BTN_DANGER,
                       ks_rect_contains(confirm_btn, mouse_x, mouse_y), false, 12.5f, 9.0f);
    }
}

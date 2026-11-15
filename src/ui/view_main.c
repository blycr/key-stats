#include "ui/view_main.h"
#include "core/win32_util.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static KsLang current_lang(const KsMainView *v) {
    KsConfig cfg = ks_config_manager_get(v->cfg_mgr);
    return ks_lang_from_code(cfg.language);
}

static void close_popup_and_trim(KsMainView *v) {
    if (!v) return;
    if (v->popup != KS_POPUP_NONE) {
        v->popup = KS_POPUP_NONE;
        if (v->renderer) {
            ks_d2d_trim_memory(v->renderer, false);
        }
    }
}

static bool corner_controls_visible(const KsMainView *v) {
    return (v && v->popup == KS_POPUP_NONE &&
            v->mouse_x >= v->win_w_dip * 0.5f - 48.0f &&
            v->mouse_x <= v->win_w_dip * 0.5f + 48.0f &&
            v->mouse_y >= 0.0f &&
            v->mouse_y <= 48.0f);
}

static void compute_header_layout(KsMainView *v) {
    float h = 54.0f;
    v->rc_titlebar = (KsRect){ 0.0f, 0.0f, v->win_w_dip, h };

    float win_btn = 24.0f;
    float win_y = 17.0f;
    v->rc_min_btn = (KsRect){ v->win_w_dip * 0.5f - 28.0f, win_y, win_btn, win_btn };
    v->rc_close_btn = (KsRect){ v->rc_min_btn.x + win_btn + 4.0f, win_y, win_btn, win_btn };
    v->rc_max_btn = (KsRect){ 0.0f, 0.0f, 0.0f, 0.0f };

    float btn_h = 30.0f;
    float btn_y = 14.0f;
    float right_edge = v->win_w_dip - 20.0f;

    v->rc_more_btn = (KsRect){ right_edge - btn_h, btn_y, btn_h, btn_h };

    float live_w = 100.0f;
    v->rc_live_btn = (KsRect){ v->rc_more_btn.x - 8.0f - live_w, btn_y, live_w, btn_h };
}

static KsRect compute_more_menu_rect(const KsMainView *v) {
    float w = 198.0f;
    float h = 158.0f; /* 8 + 4*34 + 6 + 8 */
    if (v->popup == KS_POPUP_CONTEXT_MENU) {
        float x = v->context_menu_x;
        float y = v->context_menu_y;
        if (x + w > v->win_w_dip - 12.0f) x = v->win_w_dip - w - 12.0f;
        if (y + h > v->win_h_dip - 12.0f) y = v->win_h_dip - h - 12.0f;
        if (x < 12.0f) x = 12.0f;
        if (y < 12.0f) y = 12.0f;
        return (KsRect){ x, y, w, h };
    }
    float x = v->rc_more_btn.x + v->rc_more_btn.w - w;
    float y = v->rc_more_btn.y + v->rc_more_btn.h + 8.0f;
    if (x < 12.0f) x = 12.0f;
    return (KsRect){ x, y, w, h };
}

static KsRect compute_dialog_rect(const KsMainView *v, float mh) {
    float mw = 410.0f;
    if (mw > v->win_w_dip - 36.0f) mw = v->win_w_dip - 36.0f;
    if (mh > v->win_h_dip - 36.0f) mh = v->win_h_dip - 36.0f;
    return (KsRect){ (v->win_w_dip - mw) * 0.5f, (v->win_h_dip - mh) * 0.5f, mw, mh };
}

void ks_view_main_init(KsMainView *v, HWND hwnd, KsD2DRenderer *renderer,
                       KsConfigManager *cfg_mgr, KsDB *db, KsKeyboardService *kbd,
                       KsTray *tray, const char *data_dir_utf8) {
    memset(v, 0, sizeof(*v));
    v->hwnd = hwnd;
    v->renderer = renderer;
    v->cfg_mgr = cfg_mgr;
    v->db = db;
    v->kbd = kbd;
    v->tray = tray;
    v->dpi = 96.0f;
    v->win_w_dip = 1120.0f;
    v->win_h_dip = 680.0f;
    v->mouse_x = -1000.0f;
    v->mouse_y = -1000.0f;
    snprintf(v->data_dir, sizeof(v->data_dir), "%s", data_dir_utf8 ? data_dir_utf8 : "");

    ks_stats_cache_invalidate(&v->stats_cache);
    ks_settings_init(&v->settings);
    if (renderer) {
        ks_d2d_enumerate_fonts(renderer, &v->fonts);
    }

    KsConfig cfg = ks_config_manager_get(cfg_mgr);
    ks_view_main_apply_config(v, &cfg, true);

    if (hwnd) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        UINT dpi = GetDpiForWindow(hwnd);
        if (dpi < 72) dpi = 96;
        WINDOWPLACEMENT wp;
        wp.length = sizeof(wp);
        bool is_max = GetWindowPlacement(hwnd, &wp) && (wp.showCmd == SW_SHOWMAXIMIZED);
        ks_view_main_resize(v, (uint32_t)(rc.right - rc.left), (uint32_t)(rc.bottom - rc.top),
                            (float)dpi, is_max);
    } else {
        compute_header_layout(v);
    }

    ks_view_main_refresh_stats(v);
    ks_view_main_refresh_active_keys(v);
    v->need_repaint = true;
}

void ks_view_main_destroy(KsMainView *v) {
    if (!v) return;
    ks_settings_destroy(&v->settings);
    ks_d2d_free_font_list(&v->fonts);
    ks_stats_cache_invalidate(&v->stats_cache);
}

void ks_view_main_refresh_stats(KsMainView *v) {
    if (!v || !v->db) return;
    KsTodaySummary next;
    memset(&next, 0, sizeof(next));
    if (ks_stats_get_all_cached(v->db, &v->stats_cache, &next)) {
        v->summary = next;
        v->need_repaint = true;
    }
}

void ks_view_main_refresh_active_keys(KsMainView *v) {
    if (!v || !v->kbd) return;
    char buf[32][KS_KEY_NAME_CAP];
    bool more = false;
    int cnt = ks_keyboard_active_keys(v->kbd, buf, 32, &more);
    bool changed = (cnt != v->active_keys_count) || (more != v->active_has_more);
    if (!changed) {
        for (int i = 0; i < cnt; ++i) {
            if (strcmp(buf[i], v->active_keys[i]) != 0) {
                changed = true;
                break;
            }
        }
    }
    if (changed) {
        v->active_keys_count = cnt;
        v->active_has_more = more;
        if (cnt > 0) memcpy(v->active_keys, buf, (size_t)cnt * sizeof(buf[0]));
        v->need_repaint = true;
    }
}

void ks_view_main_apply_config(KsMainView *v, const KsConfig *cfg, bool immediate_theme) {
    if (!v || !cfg) return;
    bool dark = ks_resolve_dark_mode(cfg->theme);
    KsColorScheme scheme = ks_scheme_from_code(cfg->color_scheme);
    bool mica = v->mica_active;
    if (v->hwnd) {
        ks_d2d_apply_window_backdrop(v->hwnd, dark, &mica);
        v->mica_active = mica;
    }
    if (immediate_theme) {
        ks_theme_init(&v->theme, dark, scheme, mica);
    } else {
        ks_theme_set_target(&v->theme, dark, scheme, mica, ks_monotonic_ms(), false);
    }
    if (v->renderer) {
        ks_d2d_set_font_family(v->renderer, cfg->font_family);
    }
    if (v->tray) {
        ks_tray_set_state(v->tray, cfg->language, v->paused, cfg->auto_start);
    }
    ks_settings_sync_edit_theme(&v->settings, &v->theme.current, v->dpi);
    v->need_repaint = true;
}

void ks_view_main_on_system_theme_changed(KsMainView *v) {
    if (!v || !v->cfg_mgr) return;
    KsConfig cfg = ks_config_manager_get(v->cfg_mgr);
    if (_stricmp(cfg.theme, "auto") == 0 || cfg.theme[0] == '\0') {
        ks_view_main_apply_config(v, &cfg, false);
    }
}

void ks_view_main_resize(KsMainView *v, uint32_t width_px, uint32_t height_px, float dpi, bool maximized) {
    if (!v) return;
    if (dpi < 72.0f) dpi = 96.0f;
    float scale = dpi / 96.0f;
    v->dpi = dpi;
    v->maximized = maximized;
    v->win_w_dip = (float)width_px / scale;
    v->win_h_dip = (float)height_px / scale;
    compute_header_layout(v);
    if (v->renderer) {
        ks_d2d_resize(v->renderer, width_px, height_px, dpi);
    }
    v->need_repaint = true;
}

bool ks_view_main_tick(KsMainView *v, uint64_t now_ms) {
    if (!v) return false;
    if (ks_theme_tick(&v->theme, now_ms)) {
        ks_settings_sync_edit_theme(&v->settings, &v->theme.current, v->dpi);
        v->need_repaint = true;
        return true;
    }
    if (v->theme.animating) {
        v->need_repaint = true;
        return true;
    }
    return false;
}

static void draw_sidebar(KsMainView *v, KsD2DRenderer *r, const KsThemePalette *pal, KsLang lang,
                         KsRect sb_rect) {
    float gap = 12.0f;
    float pad_x = 18.0f;

    /* 1. Borderless Hero Counter Card (96px) */
    float hero_h = 96.0f;
    ks_d2d_fill_rounded_rect(r, sb_rect.x, sb_rect.y, sb_rect.w, hero_h, 16.0f, pal->surface);

    ks_d2d_draw_text(r, ks_tr(lang, KS_STR_HERO_TOTAL),
                     sb_rect.x + pad_x, sb_rect.y + 15.0f, sb_rect.w - pad_x * 2.0f, 18.0f,
                     12.0f, KS_FONT_MEDIUM, pal->text_secondary, KS_ALIGN_LEFT, true, true, NULL);

    char total_buf[32];
    ks_format_count(v->summary.total_keys, total_buf, sizeof(total_buf));
    float num_w = ks_d2d_measure_text_width(r, total_buf, 34.0f, KS_FONT_BOLD, NULL);
    float max_num_w = sb_rect.w - pad_x * 2.0f - 48.0f;
    if (num_w > max_num_w) num_w = max_num_w;

    ks_d2d_draw_text(r, total_buf,
                     sb_rect.x + pad_x, sb_rect.y + 39.0f, num_w + 4.0f, 42.0f,
                     34.0f, KS_FONT_BOLD, pal->text_primary, KS_ALIGN_LEFT, true, true, NULL);
    ks_d2d_draw_text(r, ks_tr(lang, KS_STR_UNIT_KEYS),
                     sb_rect.x + pad_x + num_w + 8.0f, sb_rect.y + 52.0f, 48.0f, 24.0f,
                     12.5f, KS_FONT_REGULAR, pal->text_tertiary, KS_ALIGN_LEFT, true, true, NULL);

    /* Remaining height divided into Compact Top Apps (5 rows max) + Flex Top Keys */
    float rem_y = sb_rect.y + hero_h + gap;
    float rem_h = sb_rect.h - hero_h - gap;
    if (rem_h < 180.0f) return;

    float apps_h = 188.0f;
    if (apps_h > rem_h * 0.45f) apps_h = rem_h * 0.45f;

    /* 2. Borderless Top Apps Card */
    KsRect apps_rc = { sb_rect.x, rem_y, sb_rect.w, apps_h };
    ks_d2d_fill_rounded_rect(r, apps_rc.x, apps_rc.y, apps_rc.w, apps_rc.h, 16.0f, pal->surface);

    ks_d2d_draw_text(r, ks_tr(lang, KS_STR_TOP_APPS),
                     apps_rc.x + pad_x, apps_rc.y + 14.0f, apps_rc.w - pad_x * 2.0f, 20.0f,
                     12.5f, KS_FONT_SEMIBOLD, pal->text_secondary, KS_ALIGN_LEFT, true, true, NULL);

    float list_y = apps_rc.y + 40.0f;
    float avail_apps_h = apps_rc.h - 52.0f;
    int max_apps = (int)(avail_apps_h / 26.0f);
    if (max_apps > 5) max_apps = 5;

    if (v->summary.app_breakdown_len == 0 || max_apps <= 0) {
        ks_d2d_draw_text(r, ks_tr(lang, KS_STR_NO_DATA),
                         apps_rc.x + pad_x, list_y, apps_rc.w - pad_x * 2.0f, avail_apps_h,
                         12.0f, KS_FONT_REGULAR, pal->text_tertiary, KS_ALIGN_CENTER, true, true, NULL);
    } else {
        int64_t top_val = v->summary.app_breakdown[0].count;
        int show = (v->summary.app_breakdown_len < max_apps) ? v->summary.app_breakdown_len : max_apps;
        for (int i = 0; i < show; ++i) {
            float ry = list_y + (float)i * 26.0f;
            char rank_buf[8];
            snprintf(rank_buf, sizeof(rank_buf), "%d", i + 1);
            ks_d2d_draw_text(r, rank_buf, apps_rc.x + pad_x, ry, 14.0f, 16.0f,
                             11.0f, KS_FONT_MEDIUM, pal->text_tertiary, KS_ALIGN_LEFT, true, true, NULL);

            const char *name = v->summary.app_breakdown[i].app_name;
            if (!name || name[0] == '\0' || _stricmp(name, "Unknown") == 0) {
                name = ks_tr(lang, KS_STR_UNKNOWN_APP);
            }
            char cb[24];
            ks_format_count(v->summary.app_breakdown[i].count, cb, sizeof(cb));

            ks_d2d_draw_text(r, name, apps_rc.x + pad_x + 16.0f, ry, apps_rc.w - pad_x * 2.0f - 78.0f, 16.0f,
                             12.0f, KS_FONT_MEDIUM, pal->text_primary, KS_ALIGN_LEFT, true, true, NULL);
            ks_d2d_draw_text(r, cb, apps_rc.x + apps_rc.w - pad_x - 62.0f, ry, 62.0f, 16.0f,
                             11.5f, KS_FONT_SEMIBOLD, pal->text_secondary, KS_ALIGN_RIGHT, true, true, NULL);

            float frac = (top_val > 0) ? (float)v->summary.app_breakdown[i].count / (float)top_val : 0.0f;
            ks_draw_progress_bar(r, pal, (KsRect){ apps_rc.x + pad_x + 16.0f, ry + 18.0f,
                                                   apps_rc.w - pad_x * 2.0f - 16.0f, 3.5f },
                                 frac, pal->accent);
        }
    }

    /* 3. Borderless Top Keys Card (clean direct labels without nested boxes) */
    KsRect keys_rc = { sb_rect.x, rem_y + apps_h + gap, sb_rect.w, rem_h - apps_h - gap };
    ks_d2d_fill_rounded_rect(r, keys_rc.x, keys_rc.y, keys_rc.w, keys_rc.h, 16.0f, pal->surface);

    ks_d2d_draw_text(r, ks_tr(lang, KS_STR_TOP_KEYS),
                     keys_rc.x + pad_x, keys_rc.y + 14.0f, keys_rc.w - pad_x * 2.0f, 20.0f,
                     12.5f, KS_FONT_SEMIBOLD, pal->text_secondary, KS_ALIGN_LEFT, true, true, NULL);

    list_y = keys_rc.y + 42.0f;
    float avail_keys_h = keys_rc.h - 54.0f;
    float row_stride = 28.0f;
    int max_keys = (int)(avail_keys_h / row_stride);
    if (max_keys > KS_MAX_TOP_KEYS) max_keys = KS_MAX_TOP_KEYS;

    if (v->summary.top_keys_len == 0 || max_keys <= 0) {
        ks_d2d_draw_text(r, ks_tr(lang, KS_STR_NO_DATA),
                         keys_rc.x + pad_x, list_y, keys_rc.w - pad_x * 2.0f, avail_keys_h,
                         12.0f, KS_FONT_REGULAR, pal->text_tertiary, KS_ALIGN_CENTER, true, true, NULL);
    } else {
        int64_t top_val = v->summary.top_keys[0].count;
        int show = (v->summary.top_keys_len < max_keys) ? v->summary.top_keys_len : max_keys;
        KsColor bar_col = ks_color_lerp(pal->accent, pal->heatmap, 0.35f);

        for (int i = 0; i < show; ++i) {
            float ry = list_y + (float)i * row_stride;
            char rank_buf[8];
            snprintf(rank_buf, sizeof(rank_buf), "%d", i + 1);
            ks_d2d_draw_text(r, rank_buf, keys_rc.x + pad_x, ry, 14.0f, 22.0f,
                             11.0f, KS_FONT_MEDIUM, pal->text_tertiary, KS_ALIGN_LEFT, true, true, NULL);

            const char *kname = v->summary.top_keys[i].key_name;
            float key_col_w = 56.0f;
            ks_d2d_draw_text(r, kname, keys_rc.x + pad_x + 16.0f, ry, key_col_w, 22.0f,
                             12.0f, KS_FONT_SEMIBOLD, pal->text_primary, KS_ALIGN_LEFT, true, true, NULL);

            char cb[24];
            ks_format_count(v->summary.top_keys[i].count, cb, sizeof(cb));
            float count_w = 52.0f;
            ks_d2d_draw_text(r, cb, keys_rc.x + keys_rc.w - pad_x - count_w, ry, count_w, 22.0f,
                             11.5f, KS_FONT_SEMIBOLD, pal->text_secondary, KS_ALIGN_RIGHT, true, true, NULL);

            float bar_x = keys_rc.x + pad_x + 16.0f + key_col_w + 8.0f;
            float bar_w = (keys_rc.x + keys_rc.w - pad_x - count_w - 8.0f) - bar_x;
            if (bar_w > 18.0f) {
                float frac = (top_val > 0) ? (float)v->summary.top_keys[i].count / (float)top_val : 0.0f;
                ks_draw_progress_bar(r, pal, (KsRect){ bar_x, ry + 9.0f, bar_w, 4.0f }, frac, bar_col);
            }
        }
    }
}

static void draw_floating_header(KsMainView *v, KsD2DRenderer *r, const KsThemePalette *pal, KsLang lang) {
    /* Seamless top strip: ZERO border hairline, ZERO background box, uniform vertical center y=29 */
    float pad_l = 22.0f;
    float badge_s = 26.0f;
    float badge_y = 16.0f;

    ks_d2d_draw_app_badge(r, pad_l, badge_y, badge_s, pal->accent, ks_rgba(255, 255, 255, 0.96f));
    ks_d2d_draw_text(r, ks_tr(lang, KS_STR_APP_TITLE),
                     pad_l + badge_s + 11.0f, 14.0f, 180.0f, 30.0f,
                     15.5f, KS_FONT_BOLD, pal->text_primary, KS_ALIGN_LEFT, true, true, NULL);

    bool base_interactive = (v->popup == KS_POPUP_NONE);

    /* Borderless Live / Paused pill with text centered in the full capsule */
    bool h_live = base_interactive && ks_rect_contains(v->rc_live_btn, v->mouse_x, v->mouse_y);
    KsColor dot_col = v->paused ? ks_rgba(245, 158, 11, 1.0f) : pal->success;
    ks_d2d_fill_rounded_rect(r, v->rc_live_btn.x, v->rc_live_btn.y, v->rc_live_btn.w, v->rc_live_btn.h,
                             15.0f, h_live ? pal->surface_overlay : pal->surface);
    ks_d2d_fill_circle(r, v->rc_live_btn.x + 14.0f, v->rc_live_btn.y + v->rc_live_btn.h * 0.5f,
                       6.0f, ks_color_alpha(dot_col, 0.22f));
    ks_d2d_fill_circle(r, v->rc_live_btn.x + 14.0f, v->rc_live_btn.y + v->rc_live_btn.h * 0.5f,
                       3.5f, dot_col);
    ks_d2d_draw_text(r, ks_tr(lang, v->paused ? KS_STR_PAUSED : KS_STR_LIVE),
                     v->rc_live_btn.x, v->rc_live_btn.y, v->rc_live_btn.w, v->rc_live_btn.h,
                     12.0f, KS_FONT_MEDIUM, pal->text_primary, KS_ALIGN_CENTER, true, true, NULL);

    /* Borderless More button pill */
    bool h_more = base_interactive && ks_rect_contains(v->rc_more_btn, v->mouse_x, v->mouse_y);
    ks_d2d_fill_rounded_rect(r, v->rc_more_btn.x, v->rc_more_btn.y, v->rc_more_btn.w, v->rc_more_btn.h,
                             15.0f, (h_more || v->popup == KS_POPUP_MORE_MENU) ? pal->surface_overlay : pal->surface);
    ks_d2d_draw_dots_vertical(r, v->rc_more_btn.x + v->rc_more_btn.w * 0.5f,
                              v->rc_more_btn.y + v->rc_more_btn.h * 0.5f, 14.0f, pal->text_secondary);

    /* Hidden by default; revealed smoothly on top-center hover (borderless micro pill) */
    if (corner_controls_visible(v)) {
        float px = v->rc_min_btn.x - 4.0f;
        float py = v->rc_min_btn.y - 2.0f;
        float pw = (v->rc_close_btn.x + v->rc_close_btn.w + 4.0f) - px;
        float ph = v->rc_min_btn.h + 4.0f;
        ks_d2d_fill_rounded_rect(r, px, py, pw, ph, ph * 0.5f, pal->surface_overlay);

        bool h_min = ks_rect_contains(v->rc_min_btn, v->mouse_x, v->mouse_y);
        if (h_min) {
            ks_d2d_fill_rounded_rect(r, v->rc_min_btn.x, v->rc_min_btn.y, v->rc_min_btn.w, v->rc_min_btn.h,
                                     12.0f, pal->surface_raised);
        }
        ks_d2d_draw_minimize_dash(r, v->rc_min_btn.x + v->rc_min_btn.w * 0.5f,
                                  v->rc_min_btn.y + v->rc_min_btn.h * 0.5f, 11.0f, pal->text_secondary);

        bool h_close = ks_rect_contains(v->rc_close_btn, v->mouse_x, v->mouse_y);
        if (h_close) {
            ks_d2d_fill_rounded_rect(r, v->rc_close_btn.x, v->rc_close_btn.y, v->rc_close_btn.w, v->rc_close_btn.h,
                                     12.0f, pal->danger);
        }
        ks_d2d_draw_close_x(r, v->rc_close_btn.x + v->rc_close_btn.w * 0.5f,
                            v->rc_close_btn.y + v->rc_close_btn.h * 0.5f, 11.0f,
                            h_close ? ks_rgba(255, 255, 255, 1.0f) : pal->text_secondary);
    }
}

static void draw_popups(KsMainView *v, KsD2DRenderer *r, const KsThemePalette *pal, KsLang lang) {
    if (v->popup == KS_POPUP_MORE_MENU || v->popup == KS_POPUP_CONTEXT_MENU) {
        KsRect menu = compute_more_menu_rect(v);
        ks_d2d_draw_glass_panel(r, menu.x, menu.y, menu.w, menu.h, 14.0f, 18.0f,
                                pal->glass_tint, pal->glass_border, pal->shadow_color);
        KsStringId items[4] = {
            KS_STR_MENU_STATUS, KS_STR_MENU_SETTINGS, KS_STR_MENU_MINIMIZE_TRAY, KS_STR_MENU_QUIT
        };
        int icon_kinds[4] = { 1, 2, 3, 4 };
        float y = menu.y + 8.0f;
        for (int i = 0; i < 4; ++i) {
            if (i == 2) {
                y += 6.0f;
            }
            KsRect ir = { menu.x + 6.0f, y, menu.w - 12.0f, 34.0f };
            bool hov = ks_rect_contains(ir, v->mouse_x, v->mouse_y);
            if (hov) {
                ks_d2d_fill_rounded_rect(r, ir.x, ir.y, ir.w, ir.h, 8.0f,
                                         (i == 3)
                                             ? ks_color_alpha(pal->danger, 0.14f)
                                             : pal->surface_overlay);
            }
            KsColor fg = (i == 3) && hov ? pal->danger : pal->text_primary;
            KsColor icon_col = (i == 3) && hov ? pal->danger : pal->text_secondary;
            ks_d2d_draw_menu_icon(r, icon_kinds[i], ir.x + 18.0f, ir.y + ir.h * 0.5f, 14.0f, icon_col);
            ks_d2d_draw_text(r, ks_tr(lang, items[i]), ir.x + 34.0f, ir.y, ir.w - 44.0f, ir.h,
                             12.5f, KS_FONT_MEDIUM, fg, KS_ALIGN_LEFT, true, true, NULL);
            y += 34.0f;
        }
    } else if (v->popup == KS_POPUP_STATUS_MODAL) {
        ks_draw_modal_scrim(r, v->win_w_dip, v->win_h_dip, 1.0f);
        KsRect dlg = compute_dialog_rect(v, 224.0f);
        ks_d2d_draw_glass_panel(r, dlg.x, dlg.y, dlg.w, dlg.h, 18.0f, 18.0f,
                                pal->glass_tint, pal->glass_border, pal->shadow_color);

        ks_d2d_draw_text(r, ks_tr(lang, KS_STR_STATUS_TITLE),
                         dlg.x + 24.0f, dlg.y + 20.0f, dlg.w - 48.0f, 26.0f,
                         17.0f, KS_FONT_BOLD, pal->text_primary, KS_ALIGN_LEFT, true, true, NULL);

        KsColor dot_col = v->paused ? ks_rgba(245, 158, 11, 1.0f) : pal->success;
        ks_d2d_fill_circle(r, dlg.x + 30.0f, dlg.y + 66.0f, 4.5f, dot_col);
        ks_d2d_draw_text(r, ks_tr(lang, v->paused ? KS_STR_STATUS_PAUSED : KS_STR_STATUS_ACTIVE),
                         dlg.x + 42.0f, dlg.y + 54.0f, dlg.w - 66.0f, 24.0f,
                         13.5f, KS_FONT_SEMIBOLD, pal->text_primary, KS_ALIGN_LEFT, true, true, NULL);

        char cnt_buf[32], info_line[128];
        ks_format_count(v->summary.total_keys, cnt_buf, sizeof(cnt_buf));
        snprintf(info_line, sizeof(info_line), "%s: %s",
                 ks_tr(lang, KS_STR_STATUS_TOTAL_LABEL), cnt_buf);
        ks_d2d_draw_text(r, info_line, dlg.x + 24.0f, dlg.y + 88.0f, dlg.w - 48.0f, 20.0f,
                         12.5f, KS_FONT_REGULAR, pal->text_secondary, KS_ALIGN_LEFT, true, true, NULL);

        ks_d2d_draw_text(r, ks_tr(lang, KS_STR_STATUS_DESC),
                         dlg.x + 24.0f, dlg.y + 114.0f, dlg.w - 48.0f, 42.0f,
                         12.0f, KS_FONT_REGULAR, pal->text_tertiary, KS_ALIGN_LEFT, true, false, NULL);

        KsRect ok_btn = { dlg.x + dlg.w - 24.0f - 104.0f, dlg.y + dlg.h - 20.0f - 34.0f, 104.0f, 34.0f };
        ks_draw_button(r, pal, ok_btn, ks_tr(lang, KS_STR_BTN_OK), KS_BTN_PRIMARY,
                       ks_rect_contains(ok_btn, v->mouse_x, v->mouse_y), false, 12.5f, 9.0f);
    } else if (v->popup == KS_POPUP_SETTINGS_MODAL) {
        ks_settings_draw(&v->settings, r, pal, lang, &v->fonts,
                         v->win_w_dip, v->win_h_dip, v->mouse_x, v->mouse_y, v->dpi);
    }
}

void ks_view_main_render(KsMainView *v) {
    if (!v || !v->renderer) return;
    const KsThemePalette *pal = &v->theme.current;
    KsLang lang = current_lang(v);
    if (v->popup == KS_POPUP_SETTINGS_MODAL && v->settings.open) {
        lang = ks_lang_from_code(v->settings.draft.language);
    }

    if (!ks_d2d_begin_draw(v->renderer, pal->bg_backdrop)) return;

    /* Subtle immersive radial ambient glow behind workspace */
    ks_d2d_fill_radial_glow(v->renderer,
                            v->win_w_dip * 0.56f, v->win_h_dip * 0.42f,
                            v->win_w_dip * 0.52f, v->win_h_dip * 0.52f,
                            pal->glow_ambient);

    draw_floating_header(v, v->renderer, pal, lang);

    float pad = 20.0f;
    float gap = 14.0f;
    float body_y = 58.0f;
    float body_h = v->win_h_dip - body_y - pad;
    float sb_w = v->win_w_dip * 0.258f;
    if (sb_w < 252.0f) sb_w = 252.0f;
    if (sb_w > 324.0f) sb_w = 324.0f;

    if (body_h > 80.0f) {
        KsRect sb_rc = { pad, body_y, sb_w, body_h };
        draw_sidebar(v, v->renderer, pal, lang, sb_rc);

        KsRect kb_rc = { pad + sb_w + gap, body_y, v->win_w_dip - (pad * 2.0f + sb_w + gap), body_h };
        ks_view_keyboard_draw(v->renderer, pal, lang, kb_rc, &v->summary,
                              v->active_keys, v->active_keys_count,
                              v->mouse_x, v->mouse_y, (v->popup == KS_POPUP_NONE),
                              NULL, 0);
    }

    draw_popups(v, v->renderer, pal, lang);

    ks_d2d_end_draw(v->renderer);
    v->need_repaint = false;
}

LRESULT ks_view_main_hit_test_nc(const KsMainView *v, POINT pt_screen) {
    if (!v || !v->hwnd) return HTCLIENT;
    RECT wr;
    GetWindowRect(v->hwnd, &wr);

    if (!v->maximized) {
        const int border = (int)(6.0f * (v->dpi / 96.0f));
        bool left = pt_screen.x < wr.left + border;
        bool right = pt_screen.x >= wr.right - border;
        bool top = pt_screen.y < wr.top + border;
        bool bottom = pt_screen.y >= wr.bottom - border;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
    }

    POINT pt_client = pt_screen;
    ScreenToClient(v->hwnd, &pt_client);
    float scale = v->dpi / 96.0f;
    float x = (float)pt_client.x / scale;
    float y = (float)pt_client.y / scale;

    if (v->popup == KS_POPUP_NONE) {
        if (ks_rect_contains(v->rc_live_btn, x, y) ||
            ks_rect_contains(v->rc_more_btn, x, y)) {
            return HTCLIENT;
        }
        if (x >= v->win_w_dip * 0.5f - 48.0f && x <= v->win_w_dip * 0.5f + 48.0f && y <= 48.0f) {
            return HTCLIENT;
        }
        if (y <= 56.0f || x < 18.0f || x > v->win_w_dip - 18.0f || y > v->win_h_dip - 18.0f) {
            return HTCAPTION;
        }
    }
    return HTCLIENT;
}

void ks_view_main_on_mouse_move(KsMainView *v, float x_dip, float y_dip) {
    if (!v) return;
    v->mouse_x = x_dip;
    v->mouse_y = y_dip;
    v->need_repaint = true;
}

void ks_view_main_on_lbutton_down(KsMainView *v, float x_dip, float y_dip) {
    if (!v) return;
    v->mouse_x = x_dip;
    v->mouse_y = y_dip;
    v->mouse_down = true;
    v->need_repaint = true;
}

static void execute_menu_action(KsMainView *v, int item_idx) {
    if (item_idx == 0) {
        v->popup = KS_POPUP_STATUS_MODAL;
    } else if (item_idx == 1) {
        KsConfig cfg = ks_config_manager_get(v->cfg_mgr);
        v->popup = KS_POPUP_SETTINGS_MODAL;
        ks_settings_open(&v->settings, &cfg, v->data_dir, &v->fonts, v->hwnd);
    } else if (item_idx == 2) {
        close_popup_and_trim(v);
        if (v->hwnd) ShowWindow(v->hwnd, SW_HIDE);
    } else if (item_idx == 3) {
        close_popup_and_trim(v);
        if (v->hwnd) PostMessageW(v->hwnd, WM_APP + 40, 0, 0); /* KS_WM_APP_QUIT */
    }
}

void ks_view_main_on_lbutton_up(KsMainView *v, float x_dip, float y_dip) {
    if (!v) return;
    v->mouse_x = x_dip;
    v->mouse_y = y_dip;
    v->mouse_down = false;
    v->need_repaint = true;

    if (v->popup == KS_POPUP_NONE) {
        if (corner_controls_visible(v)) {
            if (ks_rect_contains(v->rc_min_btn, x_dip, y_dip)) {
                if (v->hwnd) ShowWindow(v->hwnd, SW_MINIMIZE);
                return;
            }
            if (ks_rect_contains(v->rc_close_btn, x_dip, y_dip)) {
                if (v->hwnd) ShowWindow(v->hwnd, SW_HIDE);
                return;
            }
        }
        if (ks_rect_contains(v->rc_live_btn, x_dip, y_dip)) {
            v->paused = !v->paused;
            if (v->kbd) ks_keyboard_set_paused(v->kbd, v->paused);
            if (v->tray && v->cfg_mgr) {
                KsConfig cfg = ks_config_manager_get(v->cfg_mgr);
                ks_tray_set_state(v->tray, cfg.language, v->paused, cfg.auto_start);
            }
            return;
        }
        if (ks_rect_contains(v->rc_more_btn, x_dip, y_dip)) {
            v->popup = KS_POPUP_MORE_MENU;
            return;
        }
        return;
    }

    if (v->popup == KS_POPUP_MORE_MENU || v->popup == KS_POPUP_CONTEXT_MENU) {
        KsRect menu = compute_more_menu_rect(v);
        if (ks_rect_contains(menu, x_dip, y_dip)) {
            float y = menu.y + 8.0f;
            for (int i = 0; i < 4; ++i) {
                if (i == 2) y += 6.0f;
                KsRect ir = { menu.x + 6.0f, y, menu.w - 12.0f, 34.0f };
                if (ks_rect_contains(ir, x_dip, y_dip)) {
                    execute_menu_action(v, i);
                    return;
                }
                y += 34.0f;
            }
        }
        close_popup_and_trim(v);
        return;
    }

    if (v->popup == KS_POPUP_STATUS_MODAL) {
        KsRect dlg = compute_dialog_rect(v, 224.0f);
        KsRect ok_btn = { dlg.x + dlg.w - 24.0f - 104.0f, dlg.y + dlg.h - 20.0f - 34.0f, 104.0f, 34.0f };
        if (!ks_rect_contains(dlg, x_dip, y_dip) || ks_rect_contains(ok_btn, x_dip, y_dip)) {
            close_popup_and_trim(v);
        }
        return;
    }

    if (v->popup == KS_POPUP_SETTINGS_MODAL) {
        int res = ks_settings_handle_click(&v->settings, v->renderer, &v->fonts,
                                           v->hwnd, v->win_w_dip, v->win_h_dip, x_dip, y_dip);
        if (res == 1) {
            /* Live preview draft theme & color scheme while user browses inside Settings */
            ks_view_main_apply_config(v, &v->settings.draft, false);
        } else if (res == 2) {
            if (v->cfg_mgr) {
                bool autostart_changed = false;
                ks_config_manager_update(v->cfg_mgr, &v->settings.draft, &autostart_changed, NULL);
                KsConfig committed = ks_config_manager_get(v->cfg_mgr);
                if (autostart_changed) {
                    ks_apply_autostart(committed.auto_start);
                }
                ks_view_main_apply_config(v, &committed, false);
            }
            close_popup_and_trim(v);
        } else if (res == 3) {
            if (v->cfg_mgr) {
                KsConfig committed = ks_config_manager_get(v->cfg_mgr);
                ks_view_main_apply_config(v, &committed, false);
            }
            close_popup_and_trim(v);
        } else if (res == 4) {
            if (v->db) ks_db_reset(v->db);
            ks_stats_cache_invalidate(&v->stats_cache);
            ks_view_main_refresh_stats(v);
        }
    }
}

void ks_view_main_on_rbutton_up(KsMainView *v, float x_dip, float y_dip) {
    if (!v) return;
    if (v->popup == KS_POPUP_NONE || v->popup == KS_POPUP_MORE_MENU ||
        v->popup == KS_POPUP_CONTEXT_MENU) {
        v->context_menu_x = x_dip;
        v->context_menu_y = y_dip;
        v->popup = KS_POPUP_CONTEXT_MENU;
        v->need_repaint = true;
    }
}

void ks_view_main_on_wheel(KsMainView *v, float x_dip, float y_dip, int delta) {
    if (!v) return;
    if (v->popup == KS_POPUP_SETTINGS_MODAL) {
        if (ks_settings_handle_wheel(&v->settings, x_dip, y_dip, v->win_w_dip, v->win_h_dip, delta)) {
            v->need_repaint = true;
        }
    }
}

void ks_view_main_on_key_down(KsMainView *v, WPARAM vk) {
    if (!v) return;
    if (vk == VK_ESCAPE && v->popup != KS_POPUP_NONE) {
        if (v->popup == KS_POPUP_SETTINGS_MODAL) {
            if (v->settings.confirm_clear_open) {
                v->settings.confirm_clear_open = false;
            } else if (v->settings.open_drop != KS_SETTINGS_DROP_NONE) {
                ks_settings_close(&v->settings, NULL, true);
                v->settings.open = true;
                v->settings.open_drop = KS_SETTINGS_DROP_NONE;
            } else {
                ks_settings_close(&v->settings, v->renderer, false);
                if (v->cfg_mgr) {
                    KsConfig committed = ks_config_manager_get(v->cfg_mgr);
                    ks_view_main_apply_config(v, &committed, false);
                }
                close_popup_and_trim(v);
            }
        } else {
            close_popup_and_trim(v);
        }
        v->need_repaint = true;
    }
}

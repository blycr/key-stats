#ifndef KS_UI_VIEW_MAIN_H
#define KS_UI_VIEW_MAIN_H

#include "core/config.h"
#include "core/db.h"
#include "core/keyboard.h"
#include "core/stats.h"
#include "core/tray.h"
#include "ui/d2d_renderer.h"
#include "ui/i18n.h"
#include "ui/theme.h"
#include "ui/view_keyboard.h"
#include "ui/view_settings.h"
#include "ui/widgets.h"

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    KS_POPUP_NONE = 0,
    KS_POPUP_MORE_MENU = 1,
    KS_POPUP_CONTEXT_MENU = 2,
    KS_POPUP_STATUS_MODAL = 3,
    KS_POPUP_SETTINGS_MODAL = 4
} KsPopupKind;

typedef struct {
    HWND hwnd;
    KsD2DRenderer *renderer;
    KsConfigManager *cfg_mgr;
    KsDB *db;
    KsKeyboardService *kbd;
    KsTray *tray;

    KsStatsCache stats_cache;
    KsTodaySummary summary;
    bool paused;
    char data_dir[MAX_PATH * 3];

    char active_keys[32][KS_KEY_NAME_CAP];
    int active_keys_count;
    bool active_has_more;

    KsThemeAnimator theme;
    KsFontList fonts;
    KsPopupKind popup;
    KsSettingsModal settings;

    float context_menu_x;
    float context_menu_y;

    float win_w_dip;
    float win_h_dip;
    float dpi;
    float mouse_x;
    float mouse_y;
    bool mouse_down;
    bool maximized;
    bool mica_active;
    bool need_repaint;

    /* Titlebar / button hit regions in DIPs */
    KsRect rc_titlebar;
    KsRect rc_live_btn;
    KsRect rc_more_btn;
    KsRect rc_min_btn;
    KsRect rc_max_btn;
    KsRect rc_close_btn;
} KsMainView;

void ks_view_main_init(KsMainView *v, HWND hwnd, KsD2DRenderer *renderer,
                       KsConfigManager *cfg_mgr, KsDB *db, KsKeyboardService *kbd,
                       KsTray *tray, const char *data_dir_utf8);
void ks_view_main_destroy(KsMainView *v);

void ks_view_main_refresh_stats(KsMainView *v);
void ks_view_main_refresh_active_keys(KsMainView *v);
void ks_view_main_apply_config(KsMainView *v, const KsConfig *cfg, bool immediate_theme);
void ks_view_main_on_system_theme_changed(KsMainView *v);

void ks_view_main_resize(KsMainView *v, uint32_t width_px, uint32_t height_px, float dpi, bool maximized);
bool ks_view_main_tick(KsMainView *v, uint64_t now_ms);
void ks_view_main_render(KsMainView *v);

/* Non-client hit test in screen pixels -> HTCLIENT / HTCAPTION / resize edges. */
LRESULT ks_view_main_hit_test_nc(const KsMainView *v, POINT pt_screen);

void ks_view_main_on_mouse_move(KsMainView *v, float x_dip, float y_dip);
void ks_view_main_on_lbutton_down(KsMainView *v, float x_dip, float y_dip);
void ks_view_main_on_lbutton_up(KsMainView *v, float x_dip, float y_dip);
void ks_view_main_on_rbutton_up(KsMainView *v, float x_dip, float y_dip);
void ks_view_main_on_wheel(KsMainView *v, float x_dip, float y_dip, int delta);
void ks_view_main_on_key_down(KsMainView *v, WPARAM vk);

#endif

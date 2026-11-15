#ifndef KS_UI_VIEW_SETTINGS_H
#define KS_UI_VIEW_SETTINGS_H

#include "core/config.h"
#include "ui/d2d_renderer.h"
#include "ui/i18n.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <windows.h>
#include <stdbool.h>

typedef enum {
    KS_SETTINGS_DROP_NONE = 0,
    KS_SETTINGS_DROP_LANG = 1,
    KS_SETTINGS_DROP_THEME = 2,
    KS_SETTINGS_DROP_SCHEME = 3,
    KS_SETTINGS_DROP_FONT = 4
} KsSettingsDropdown;

typedef struct {
    bool open;
    bool confirm_clear_open;
    KsConfig draft;
    KsConfig original;
    char data_dir[MAX_PATH * 3];

    KsSettingsDropdown open_drop;
    char font_query[128];
    int *filtered_idx;
    int filtered_count;
    int filtered_cap;
    float font_scroll_y;
    int hover_drop_item;
    int hover_control;

    HWND search_edit;
    HFONT search_hfont;
    HBRUSH search_bg_brush;
    KsRect search_box_rect;
} KsSettingsModal;

void ks_settings_init(KsSettingsModal *m);
void ks_settings_destroy(KsSettingsModal *m);

void ks_settings_open(KsSettingsModal *m, const KsConfig *cfg, const char *data_dir_utf8,
                      const KsFontList *fonts, HWND parent_hwnd);
void ks_settings_close(KsSettingsModal *m, KsD2DRenderer *r, bool apply_changes);

void ks_settings_filter_fonts(KsSettingsModal *m, const KsFontList *fonts, const char *query_utf8);
void ks_settings_visible_range(int total_items, float scroll_y, float view_h, float item_h,
                               int *out_start, int *out_end);

void ks_settings_on_search_changed(KsSettingsModal *m, const KsFontList *fonts);
bool ks_settings_handle_wheel(KsSettingsModal *m, float mx, float my, float win_w, float win_h, int delta);

/* Returns: 0 = nothing, 1 = keep open (repaint/live preview), 2 = saved & closed, 3 = cancelled & closed, 4 = clear stats confirmed */
int ks_settings_handle_click(KsSettingsModal *m, KsD2DRenderer *r, const KsFontList *fonts,
                             HWND parent_hwnd, float win_w, float win_h, float mx, float my);

void ks_settings_sync_edit_theme(KsSettingsModal *m, const KsThemePalette *pal, float dpi);
HBRUSH ks_settings_ctlcolor_edit(KsSettingsModal *m, HDC hdc, const KsThemePalette *pal);

void ks_settings_draw(KsSettingsModal *m, KsD2DRenderer *r, const KsThemePalette *pal,
                      KsLang lang, const KsFontList *fonts,
                      float win_w, float win_h, float mouse_x, float mouse_y, float dpi);

#endif

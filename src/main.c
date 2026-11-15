#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <objbase.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"
#include "core/db.h"
#include "core/keyboard.h"
#include "core/tray.h"
#include "core/win32_util.h"
#include "resources/resource.h"
#include "ui/d2d_renderer.h"
#include "ui/view_main.h"

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

#define KS_WM_APP_QUIT (WM_APP + 40)
#define KS_TIMER_THEME 1
#define KS_TIMER_KEYS  2
#define KS_TIMER_SIZE  3
#define KS_TIMER_TRIM  4

typedef struct {
    HWND hwnd;
    KsConfigManager cfg_mgr;
    KsDB *db;
    KsKeyboardService *kbd;
    KsTray *tray;
    KsD2DRenderer *renderer;
    KsMainView view;
    bool initialized;
    bool tracking_mouse;
    int pending_save_x;
    int pending_save_y;
    int pending_save_w;
    int pending_save_h;
    bool pending_has_pos;
} KsAppState;

static void on_db_commit(void *userdata) {
    HWND hwnd = (HWND)userdata;
    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, KS_WM_STATS_DIRTY, 0, 0);
    }
}

static void show_and_activate(KsAppState *app) {
    if (!app || !app->hwnd) return;
    if (IsIconic(app->hwnd)) {
        ShowWindow(app->hwnd, SW_RESTORE);
    } else {
        ShowWindow(app->hwnd, SW_SHOW);
    }
    SetForegroundWindow(app->hwnd);
    BringWindowToTop(app->hwnd);
    ks_view_main_refresh_stats(&app->view);
    ks_view_main_refresh_active_keys(&app->view);
    ks_view_main_render(&app->view);
    SetTimer(app->hwnd, KS_TIMER_TRIM, 900, NULL);
}

static void render_if_needed(KsAppState *app) {
    if (!app || !app->initialized) return;
    if (app->view.theme.animating) {
        SetTimer(app->hwnd, KS_TIMER_THEME, 16, NULL);
    } else {
        KillTimer(app->hwnd, KS_TIMER_THEME);
    }
    if (app->view.need_repaint && IsWindowVisible(app->hwnd) && !IsIconic(app->hwnd)) {
        ks_view_main_render(&app->view);
        SetTimer(app->hwnd, KS_TIMER_TRIM, 900, NULL);
    }
}

static float dpi_scale_for(HWND hwnd) {
    UINT dpi = hwnd ? GetDpiForWindow(hwnd) : 96;
    if (dpi < 72) dpi = 96;
    return (float)dpi / 96.0f;
}

static LRESULT CALLBACK main_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    KsAppState *app = (KsAppState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        app = (KsAppState *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
        if (app) app->hwnd = hwnd;
        return TRUE;
    }

    case WM_NCCALCSIZE: {
        if (wParam == TRUE && lParam != 0) {
            NCCALCSIZE_PARAMS *ncp = (NCCALCSIZE_PARAMS *)lParam;
            if (IsZoomed(hwnd)) {
                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi;
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(mon, &mi)) {
                    ncp->rgrc[0] = mi.rcWork;
                }
            }
        }
        return 0;
    }
    case WM_NCPAINT: {
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mmi = (MINMAXINFO *)lParam;
        float sc = dpi_scale_for(hwnd);
        mmi->ptMinTrackSize.x = (LONG)(780.0f * sc);
        mmi->ptMinTrackSize.y = (LONG)(540.0f * sc);

        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(mon, &mi)) {
            mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
            mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        if (app && app->initialized) {
            ks_view_main_render(&app->view);
        }
        return 0;
    }

    case WM_NCHITTEST: {
        if (app && app->initialized) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            return ks_view_main_hit_test_nc(&app->view, pt);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    case WM_SIZE: {
        if (app && app->initialized) {
            if (wParam == SIZE_MINIMIZED) {
                if (app->renderer) ks_d2d_trim_memory(app->renderer, true);
                return 0;
            }
            UINT w = LOWORD(lParam);
            UINT h = HIWORD(lParam);
            UINT dpi = GetDpiForWindow(hwnd);
            if (dpi < 72) dpi = 96;
            bool is_max = (wParam == SIZE_MAXIMIZED);
            ks_view_main_resize(&app->view, w, h, (float)dpi, is_max);
            render_if_needed(app);

            if (!is_max && w > 0 && h > 0) {
                float sc = (float)dpi / 96.0f;
                RECT wr;
                if (GetWindowRect(hwnd, &wr)) {
                    app->pending_save_x = wr.left;
                    app->pending_save_y = wr.top;
                    app->pending_has_pos = true;
                }
                app->pending_save_w = (int)((float)w / sc + 0.5f);
                app->pending_save_h = (int)((float)h / sc + 0.5f);
                SetTimer(hwnd, KS_TIMER_SIZE, 350, NULL);
            }
        }
        return 0;
    }
    case WM_MOVE: {
        if (app && app->initialized && !IsIconic(hwnd) && !IsZoomed(hwnd)) {
            RECT wr;
            if (GetWindowRect(hwnd, &wr) && (wr.right - wr.left) > 0 && (wr.bottom - wr.top) > 0) {
                UINT dpi = GetDpiForWindow(hwnd);
                if (dpi < 72) dpi = 96;
                float sc = (float)dpi / 96.0f;
                app->pending_save_x = wr.left;
                app->pending_save_y = wr.top;
                app->pending_save_w = (int)((float)(wr.right - wr.left) / sc + 0.5f);
                app->pending_save_h = (int)((float)(wr.bottom - wr.top) / sc + 0.5f);
                app->pending_has_pos = true;
                SetTimer(hwnd, KS_TIMER_SIZE, 350, NULL);
            }
        }
        return 0;
    }
    case WM_EXITSIZEMOVE: {
        if (app && app->initialized && app->pending_save_w >= KS_MIN_WINDOW_WIDTH &&
            app->pending_save_h >= KS_MIN_WINDOW_HEIGHT) {
            KillTimer(hwnd, KS_TIMER_SIZE);
            ks_config_manager_set_window_rect(&app->cfg_mgr,
                app->pending_save_x, app->pending_save_y,
                app->pending_save_w, app->pending_save_h,
                app->pending_has_pos);
        }
        return 0;
    }

    case WM_SHOWWINDOW: {
        if (app && app->initialized && wParam == FALSE && app->renderer) {
            ks_d2d_trim_memory(app->renderer, true);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    case WM_DPICHANGED: {
        RECT *suggested = (RECT *)lParam;
        SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        if (app && app->initialized) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            UINT dpi = HIWORD(wParam);
            ks_view_main_resize(&app->view, (uint32_t)(rc.right - rc.left),
                                (uint32_t)(rc.bottom - rc.top), (float)dpi, IsZoomed(hwnd));
            render_if_needed(app);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (app && app->initialized) {
            if (!app->tracking_mouse) {
                TRACKMOUSEEVENT tme;
                memset(&tme, 0, sizeof(tme));
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                app->tracking_mouse = true;
            }
            float sc = dpi_scale_for(hwnd);
            ks_view_main_on_mouse_move(&app->view,
                                       (float)GET_X_LPARAM(lParam) / sc,
                                       (float)GET_Y_LPARAM(lParam) / sc);
            render_if_needed(app);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        if (app && app->initialized) {
            app->tracking_mouse = false;
            ks_view_main_on_mouse_move(&app->view, -1000.0f, -1000.0f);
            render_if_needed(app);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (app && app->initialized) {
            float sc = dpi_scale_for(hwnd);
            ks_view_main_on_lbutton_down(&app->view,
                                         (float)GET_X_LPARAM(lParam) / sc,
                                         (float)GET_Y_LPARAM(lParam) / sc);
            render_if_needed(app);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (app && app->initialized) {
            float sc = dpi_scale_for(hwnd);
            ks_view_main_on_lbutton_up(&app->view,
                                       (float)GET_X_LPARAM(lParam) / sc,
                                       (float)GET_Y_LPARAM(lParam) / sc);
            render_if_needed(app);
        }
        return 0;
    }

    case WM_RBUTTONUP: {
        if (app && app->initialized) {
            float sc = dpi_scale_for(hwnd);
            ks_view_main_on_rbutton_up(&app->view,
                                       (float)GET_X_LPARAM(lParam) / sc,
                                       (float)GET_Y_LPARAM(lParam) / sc);
            render_if_needed(app);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (app && app->initialized) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            float sc = dpi_scale_for(hwnd);
            ks_view_main_on_wheel(&app->view, (float)pt.x / sc, (float)pt.y / sc,
                                  GET_WHEEL_DELTA_WPARAM(wParam));
            render_if_needed(app);
        }
        return 0;
    }

    case WM_KEYDOWN: {
        if (app && app->initialized) {
            ks_view_main_on_key_down(&app->view, wParam);
            render_if_needed(app);
        }
        return 0;
    }

    case WM_COMMAND: {
        if (app && app->initialized && HIWORD(wParam) == EN_CHANGE &&
            (HWND)lParam == app->view.settings.search_edit) {
            ks_settings_on_search_changed(&app->view.settings, &app->view.fonts);
            app->view.need_repaint = true;
            render_if_needed(app);
            return 0;
        }
        break;
    }

    case WM_CTLCOLOREDIT: {
        if (app && app->initialized && (HWND)lParam == app->view.settings.search_edit) {
            HBRUSH br = ks_settings_ctlcolor_edit(&app->view.settings, (HDC)wParam, &app->view.theme.current);
            if (br) return (LRESULT)br;
        }
        break;
    }

    case WM_TIMER: {
        if (!app || !app->initialized) return 0;
        if (wParam == KS_TIMER_THEME) {
            ks_view_main_tick(&app->view, ks_monotonic_ms());
            render_if_needed(app);
        } else if (wParam == KS_TIMER_KEYS) {
            if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
                ks_view_main_refresh_active_keys(&app->view);
                render_if_needed(app);
            }
        } else if (wParam == KS_TIMER_SIZE) {
            KillTimer(hwnd, KS_TIMER_SIZE);
            if (app->pending_save_w >= KS_MIN_WINDOW_WIDTH &&
                app->pending_save_h >= KS_MIN_WINDOW_HEIGHT) {
                ks_config_manager_set_window_rect(&app->cfg_mgr,
                    app->pending_save_x, app->pending_save_y,
                    app->pending_save_w, app->pending_save_h,
                    app->pending_has_pos);
            }
        } else if (wParam == KS_TIMER_TRIM) {
            KillTimer(hwnd, KS_TIMER_TRIM);
            if (app->renderer && !app->view.theme.animating) {
                ks_d2d_trim_memory(app->renderer, true);
            }
        }
        return 0;
    }

    case KS_WM_KEYS_DIRTY: {
        if (app && app->initialized && app->kbd) {
            ks_keyboard_clear_notify_pending(app->kbd);
            if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
                ks_view_main_refresh_active_keys(&app->view);
                render_if_needed(app);
            }
        }
        return 0;
    }

    case KS_WM_STATS_DIRTY: {
        if (app && app->initialized && IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
            ks_view_main_refresh_stats(&app->view);
            render_if_needed(app);
        }
        return 0;
    }

    case WM_SETTINGCHANGE: {
        if (app && app->initialized) {
            ks_view_main_on_system_theme_changed(&app->view);
            render_if_needed(app);
        }
        return 0;
    }

    case KS_WM_SHOW_EXISTING:
    case KS_WM_TRAY_SHOW: {
        show_and_activate(app);
        return 0;
    }

    case KS_WM_TRAY_TOGGLE_PAUSE: {
        if (app && app->initialized) {
            bool paused = !ks_keyboard_is_paused(app->kbd);
            ks_keyboard_set_paused(app->kbd, paused);
            KsConfig cfg = ks_config_manager_get(&app->cfg_mgr);
            ks_tray_set_state(app->tray, cfg.language, paused, cfg.auto_start);
            app->view.need_repaint = true;
            render_if_needed(app);
        }
        return 0;
    }
    case KS_WM_TRAY_TOGGLE_AUTOSTART: {
        if (app && app->initialized) {
            KsConfig cfg = ks_config_manager_get(&app->cfg_mgr);
            cfg.auto_start = !cfg.auto_start;
            ks_config_manager_update(&app->cfg_mgr, &cfg, NULL, NULL);
            ks_apply_autostart(cfg.auto_start);
            bool paused = ks_keyboard_is_paused(app->kbd);
            ks_tray_set_state(app->tray, cfg.language, paused, cfg.auto_start);
            app->view.settings.draft.auto_start = cfg.auto_start;
            app->view.need_repaint = true;
            render_if_needed(app);
        }
        return 0;
    }
    case KS_WM_TRAY_RESET_SETTINGS: {
        if (app && app->initialized) {
            ks_config_manager_reset_defaults(&app->cfg_mgr);
            KsConfig cfg = ks_config_manager_get(&app->cfg_mgr);
            ks_apply_autostart(cfg.auto_start);
            if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);
            UINT dpi = GetDpiForWindow(hwnd);
            if (dpi < 72) dpi = 96;
            int sw = MulDiv(cfg.window_width, (int)dpi, 96);
            int sh = MulDiv(cfg.window_height, (int)dpi, 96);
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi;
            mi.cbSize = sizeof(mi);
            int nx = 0, ny = 0;
            if (GetMonitorInfoW(mon, &mi)) {
                int ww = mi.rcWork.right - mi.rcWork.left;
                int wh = mi.rcWork.bottom - mi.rcWork.top;
                if (sw > ww) sw = ww;
                if (sh > wh) sh = wh;
                nx = mi.rcWork.left + (ww - sw) / 2;
                ny = mi.rcWork.top + (wh - sh) / 2;
            }
            SetWindowPos(hwnd, NULL, nx, ny, sw, sh, SWP_NOZORDER | SWP_NOACTIVATE);
            KillTimer(hwnd, KS_TIMER_SIZE);
            app->pending_save_x = nx;
            app->pending_save_y = ny;
            app->pending_save_w = cfg.window_width;
            app->pending_save_h = cfg.window_height;
            app->pending_has_pos = false;
            ks_view_main_apply_config(&app->view, &cfg, false);
            if (app->view.popup == KS_POPUP_SETTINGS_MODAL) {
                ks_settings_open(&app->view.settings, &cfg, app->view.data_dir, &app->view.fonts, hwnd);
            }
            bool paused = ks_keyboard_is_paused(app->kbd);
            ks_tray_set_state(app->tray, cfg.language, paused, cfg.auto_start);
            SetTimer(hwnd, KS_TIMER_THEME, 16, NULL);
            app->view.need_repaint = true;
            render_if_needed(app);
        }
        return 0;
    }
    case KS_WM_TRAY_EXIT:
    case KS_WM_APP_QUIT: {
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_CLOSE: {
        ShowWindow(hwnd, SW_HIDE);
        if (app && app->renderer) {
            ks_d2d_trim_memory(app->renderer, true);
        }
        return 0;
    }

    case WM_DESTROY: {
        if (app && app->initialized && app->pending_save_w >= KS_MIN_WINDOW_WIDTH &&
            app->pending_save_h >= KS_MIN_WINDOW_HEIGHT) {
            ks_config_manager_set_window_rect(&app->cfg_mgr,
                app->pending_save_x, app->pending_save_y,
                app->pending_save_w, app->pending_save_h,
                app->pending_has_pos);
        }
        KillTimer(hwnd, KS_TIMER_THEME);
        KillTimer(hwnd, KS_TIMER_KEYS);
        KillTimer(hwnd, KS_TIMER_SIZE);
        KillTimer(hwnd, KS_TIMER_TRIM);
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static int run_snapshot_mode(const wchar_t *out_dir_w) {
    KsD2DRenderer *r = ks_d2d_create(NULL);
    if (!r) return 2;

    wchar_t tmp_dir[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp_dir);
    wchar_t data_dir[MAX_PATH], env_path[MAX_PATH];
    swprintf(data_dir, MAX_PATH, L"%lskeystats_snap_%lu", tmp_dir, (unsigned long)GetCurrentProcessId());
    swprintf(env_path, MAX_PATH, L"%ls\\.env", data_dir);
    ks_mkdir_recursive_w(data_dir);

    KsConfigManager cfg_mgr;
    ks_config_manager_init_custom(&cfg_mgr, env_path, data_dir, false);

    char *data_u8 = ks_wide_to_utf8_alloc(data_dir);
    KsDB *db = ks_db_init(data_dir);
    if (db) {
        int64_t now = ks_now_unix_ms();
        const int vks[] = { 'E', 'E', 'E', 'A', 'A', 'S', 'T', 'I', 'N', 32, 32, 32, 32, 13, 160, 162, 'C', 'V' };
        for (int i = 0; i < 120; ++i) {
            ks_db_push_event(db, vks[i % (int)(sizeof(vks) / sizeof(vks[0]))],
                             (i % 3 == 0) ? "Visual Studio Code" : ((i % 3 == 1) ? "Google Chrome" : "Windows Terminal"),
                             now - (int64_t)i * 1000);
        }
        Sleep(350);
    }

    KsMainView v;
    ks_view_main_init(&v, NULL, r, &cfg_mgr, db, NULL, NULL, data_u8);
    ks_view_main_resize(&v, 1120, 680, 96.0f, false);
    ks_view_main_refresh_stats(&v);
    snprintf(v.active_keys[0], KS_KEY_NAME_CAP, "E");
    snprintf(v.active_keys[1], KS_KEY_NAME_CAP, "Space");
    v.active_keys_count = 2;

    ks_mkdir_recursive_w(out_dir_w);

    /* 1. Dark Mode Main Screen (with Space key hover tooltip visible) */
    ks_theme_init(&v.theme, true, KS_SCHEME_INDIGO, false);
    v.mouse_x = 670.0f;
    v.mouse_y = 515.0f;
    ks_view_main_render(&v);
    wchar_t png_path[MAX_PATH];
    swprintf(png_path, MAX_PATH, L"%ls\\01_dark_main.png", out_dir_w);
    ks_d2d_save_snapshot_png_w(r, png_path);

    /* 2. Light Mode Main Screen */
    v.mouse_x = -1000.0f;
    v.mouse_y = -1000.0f;
    ks_theme_init(&v.theme, false, KS_SCHEME_INDIGO, false);
    ks_view_main_render(&v);
    swprintf(png_path, MAX_PATH, L"%ls\\02_light_main.png", out_dir_w);
    ks_d2d_save_snapshot_png_w(r, png_path);

    /* 3. Dark Mode + More Menu Gaussian Blur */
    ks_theme_init(&v.theme, true, KS_SCHEME_INDIGO, false);
    v.popup = KS_POPUP_MORE_MENU;
    ks_view_main_render(&v);
    swprintf(png_path, MAX_PATH, L"%ls\\03_dark_blur_menu.png", out_dir_w);
    ks_d2d_save_snapshot_png_w(r, png_path);

    /* 4. Dark Mode + Settings Modal + Searchable Font Dropdown */
    KsConfig cfg = ks_config_manager_get(&cfg_mgr);
    v.popup = KS_POPUP_SETTINGS_MODAL;
    ks_settings_open(&v.settings, &cfg, data_u8, &v.fonts, NULL);
    v.settings.open_drop = KS_SETTINGS_DROP_FONT;
    ks_view_main_render(&v);
    swprintf(png_path, MAX_PATH, L"%ls\\04_settings_font_dropdown.png", out_dir_w);
    ks_d2d_save_snapshot_png_w(r, png_path);

    ks_view_main_destroy(&v);
    ks_d2d_destroy(r);
    if (db) ks_db_close(db);
    free(data_u8);

    wchar_t tmp_pat[MAX_PATH];
    swprintf(tmp_pat, MAX_PATH, L"%ls\\*", data_dir);
    WIN32_FIND_DATAW fd;
    HANDLE hf = FindFirstFileW(tmp_pat, &fd);
    if (hf != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                wchar_t fp[MAX_PATH];
                swprintf(fp, MAX_PATH, L"%ls\\%ls", data_dir, fd.cFileName);
                DeleteFileW(fp);
            }
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }
    RemoveDirectoryW(data_dir);
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)nCmdShow;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(lpCmdLine && lpCmdLine[0] ? GetCommandLineW() : L"", &argc);
    if (argv) {
        for (int i = 1; i + 1 < argc; ++i) {
            if (wcscmp(argv[i], L"--snapshot-dir") == 0) {
                int rc = run_snapshot_mode(argv[i + 1]);
                LocalFree(argv);
                CoUninitialize();
                return rc;
            }
        }
        LocalFree(argv);
    }

    bool acquired = false;
    HANDLE mutex = ks_acquire_single_instance(KS_SINGLE_INSTANCE_MUTEX, &acquired);
    if (!acquired) {
        if (mutex) CloseHandle(mutex);
        ks_focus_existing_window();
        CoUninitialize();
        return 0;
    }

    KsAppState *app = (KsAppState *)calloc(1, sizeof(KsAppState));
    if (!app) {
        CloseHandle(mutex);
        CoUninitialize();
        return 1;
    }

    ks_config_manager_init(&app->cfg_mgr);
    KsConfig cfg = ks_config_manager_get(&app->cfg_mgr);

    char *data_dir_u8 = ks_wide_to_utf8_alloc(app->cfg_mgr.data_dir);

    app->db = ks_db_init(app->cfg_mgr.data_dir);
    app->kbd = ks_keyboard_create(app->db);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = main_wnd_proc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = KS_MAIN_WINDOW_CLASS;
    RegisterClassExW(&wc);

    int init_w = cfg.window_width >= KS_MIN_WINDOW_WIDTH ? cfg.window_width : KS_DEFAULT_WINDOW_WIDTH;
    int init_h = cfg.window_height >= KS_MIN_WINDOW_HEIGHT ? cfg.window_height : KS_DEFAULT_WINDOW_HEIGHT;
    int init_x = cfg.has_window_pos ? cfg.window_x : 0;
    int init_y = cfg.has_window_pos ? cfg.window_y : 0;

    if (cfg.auto_start) {
        ks_apply_autostart(true);
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP,
        KS_MAIN_WINDOW_CLASS,
        L"KeyStats",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU,
        init_x, init_y, init_w, init_h,
        NULL, NULL, hInstance, app);

    if (!hwnd) {
        if (app->kbd) ks_keyboard_destroy(app->kbd);
        if (app->db) ks_db_close(app->db);
        free(data_dir_u8);
        free(app);
        CloseHandle(mutex);
        CoUninitialize();
        return 1;
    }

    /* Scale window geometry cleanly for target monitor DPI and restore clamped position. */
    UINT dpi = GetDpiForWindow(hwnd);
    if (dpi < 72) dpi = 96;
    int sw = MulDiv(init_w, (int)dpi, 96);
    int sh = MulDiv(init_h, (int)dpi, 96);
    POINT ref_pt = { init_x, init_y };
    HMONITOR mon = cfg.has_window_pos
        ? MonitorFromPoint(ref_pt, MONITOR_DEFAULTTONEAREST)
        : MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    int final_x = init_x;
    int final_y = init_y;
    if (GetMonitorInfoW(mon, &mi)) {
        int ww = mi.rcWork.right - mi.rcWork.left;
        int wh = mi.rcWork.bottom - mi.rcWork.top;
        if (sw > ww) sw = ww;
        if (sh > wh) sh = wh;
        if (cfg.has_window_pos) {
            if (final_x < mi.rcWork.left) final_x = mi.rcWork.left;
            if (final_y < mi.rcWork.top) final_y = mi.rcWork.top;
            if (final_x + sw > mi.rcWork.right) final_x = mi.rcWork.right - sw;
            if (final_y + sh > mi.rcWork.bottom) final_y = mi.rcWork.bottom - sh;
        } else {
            final_x = mi.rcWork.left + (ww - sw) / 2;
            final_y = mi.rcWork.top + (wh - sh) / 2;
        }
    }
    SetWindowPos(hwnd, NULL, final_x, final_y, sw, sh,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    app->pending_save_x = final_x;
    app->pending_save_y = final_y;
    app->pending_save_w = init_w;
    app->pending_save_h = init_h;
    app->pending_has_pos = cfg.has_window_pos;

    if (app->db) {
        ks_db_set_commit_callback(app->db, on_db_commit, hwnd);
    }
    if (app->kbd) {
        ks_keyboard_set_notify_hwnd(app->kbd, hwnd);
        ks_keyboard_start(app->kbd);
    }

    app->tray = ks_tray_start(hwnd, cfg.language, false, cfg.auto_start);
    app->renderer = ks_d2d_create(hwnd);
    ks_view_main_init(&app->view, hwnd, app->renderer, &app->cfg_mgr, app->db, app->kbd, app->tray, data_dir_u8);
    app->initialized = true;

    /* Render initial frame BEFORE showing window so first presentation is zero-flicker. */
    SetTimer(hwnd, KS_TIMER_KEYS, 45, NULL);

    if (!cfg.start_minimized) {
        ks_view_main_render(&app->view);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        SetTimer(hwnd, KS_TIMER_TRIM, 900, NULL);
    } else if (app->renderer) {
        ks_d2d_trim_memory(app->renderer, true);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    /* Clean shutdown order: tray -> keyboard -> UI -> DB -> config -> mutex */
    app->initialized = false;
    if (app->tray) ks_tray_stop(app->tray);
    if (app->kbd) ks_keyboard_stop(app->kbd);
    ks_view_main_destroy(&app->view);
    if (app->renderer) ks_d2d_destroy(app->renderer);
    if (app->db) ks_db_close(app->db);
    if (app->kbd) ks_keyboard_destroy(app->kbd);
    free(data_dir_u8);
    free(app);

    if (mutex) CloseHandle(mutex);
    CoUninitialize();
    return (int)msg.wParam;
}

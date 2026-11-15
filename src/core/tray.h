#ifndef KS_TRAY_H
#define KS_TRAY_H

#include "win32_util.h"
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KS_WM_TRAY_SHOW           (WM_APP + 30)
#define KS_WM_TRAY_EXIT           (WM_APP + 31)
#define KS_WM_TRAY_TOGGLE_PAUSE   (WM_APP + 32)
#define KS_WM_TRAY_TOGGLE_AUTOSTART (WM_APP + 33)
#define KS_WM_TRAY_RESET_SETTINGS (WM_APP + 34)

typedef struct {
    HWND main_hwnd;
    HWND tray_hwnd;
    HANDLE thread;
    HANDLE ready_ev;
    HICON hicon;
    atomic_bool is_zh;
    atomic_bool paused;
    atomic_bool auto_start;
    bool quit_sent;
} KsTray;

KsTray *ks_tray_start(HWND main_hwnd, const char *language, bool paused, bool auto_start);
void ks_tray_set_language(KsTray *tray, const char *language);
void ks_tray_set_state(KsTray *tray, const char *language, bool paused, bool auto_start);
void ks_tray_stop(KsTray *tray);

#ifdef __cplusplus
}
#endif

#endif // KS_TRAY_H

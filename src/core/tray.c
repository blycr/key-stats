#include "tray.h"
#include "../resources/resource.h"
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>

#define KS_TRAY_CALLBACK_MSG    (WM_USER + 1)
#define KS_TRAY_APP_QUIT        (WM_USER + 2)
#define KS_TRAY_ID_SHOW         1001
#define KS_TRAY_ID_QUIT         1002
#define KS_TRAY_ID_PAUSE        1003
#define KS_TRAY_ID_AUTOSTART    1004
#define KS_TRAY_ID_RESET_CFG    1005

static bool resolve_is_zh(const char *language) {
    if (language && strcmp(language, "zh") == 0) return true;
    if (language && strcmp(language, "en") == 0) return false;
    LANGID lid = GetUserDefaultUILanguage();
    return (lid & 0x3FF) == 0x04;
}

static void add_tray_icon(KsTray *tray, HWND hwnd) {
    NOTIFYICONDATAW nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = KS_TRAY_CALLBACK_MSG;
    nid.hIcon = tray->hicon;
    wcsncpy(nid.szTip, L"KeyStats", 127);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

static void remove_tray_icon(HWND hwnd) {
    NOTIFYICONDATAW nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

static void show_tray_context_menu(KsTray *tray, HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    bool zh = atomic_load(&tray->is_zh);
    bool paused = atomic_load(&tray->paused);
    bool auto_start = atomic_load(&tray->auto_start);

    AppendMenuW(menu, MF_STRING, KS_TRAY_ID_SHOW,
                zh ? L"\x663E\x793A\x4E3B\x754C\x9762" : L"Show Window");
    AppendMenuW(menu, MF_STRING, KS_TRAY_ID_PAUSE,
                paused ? (zh ? L"\x6062\x590D\x76D1\x542C" : L"Resume Recording")
                       : (zh ? L"\x6682\x505C\x76D1\x542C" : L"Pause Recording"));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING | (auto_start ? MF_CHECKED : MF_UNCHECKED), KS_TRAY_ID_AUTOSTART,
                zh ? L"\x5F00\x673A\x81EA\x542F" : L"Launch on Startup");
    AppendMenuW(menu, MF_STRING, KS_TRAY_ID_RESET_CFG,
                zh ? L"\x91CD\x7F6E\x5E03\x5C40\x4E0E\x8BBE\x7F6E" : L"Reset Layout & Settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, KS_TRAY_ID_QUIT, zh ? L"\x9000\x51FA" : L"Quit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

static LRESULT CALLBACK tray_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    KsTray *tray = (KsTray *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        tray = (KsTray *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)tray);
        add_tray_icon(tray, hwnd);
        return 0;
    }
    case KS_TRAY_CALLBACK_MSG:
        if (!tray) return 0;
        if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
            if (tray->main_hwnd) PostMessageW(tray->main_hwnd, KS_WM_TRAY_SHOW, 0, 0);
        } else if (lParam == WM_RBUTTONUP) {
            show_tray_context_menu(tray, hwnd);
        }
        return 0;
    case WM_COMMAND:
        if (!tray || !tray->main_hwnd) return 0;
        switch (LOWORD(wParam)) {
        case KS_TRAY_ID_SHOW:
            PostMessageW(tray->main_hwnd, KS_WM_TRAY_SHOW, 0, 0);
            break;
        case KS_TRAY_ID_PAUSE:
            PostMessageW(tray->main_hwnd, KS_WM_TRAY_TOGGLE_PAUSE, 0, 0);
            break;
        case KS_TRAY_ID_AUTOSTART:
            PostMessageW(tray->main_hwnd, KS_WM_TRAY_TOGGLE_AUTOSTART, 0, 0);
            break;
        case KS_TRAY_ID_RESET_CFG:
            PostMessageW(tray->main_hwnd, KS_WM_TRAY_RESET_SETTINGS, 0, 0);
            break;
        case KS_TRAY_ID_QUIT:
            PostMessageW(tray->main_hwnd, KS_WM_TRAY_EXIT, 0, 0);
            break;
        }
        return 0;
    case KS_TRAY_APP_QUIT:
        remove_tray_icon(hwnd);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        remove_tray_icon(hwnd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static DWORD WINAPI tray_thread_proc(LPVOID param) {
    KsTray *tray = (KsTray *)param;
    HINSTANCE hInst = GetModuleHandleW(NULL);

    tray->hicon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                    LR_SHARED);
    if (!tray->hicon) {
        tray->hicon = LoadIconW(NULL, IDI_APPLICATION);
    }

    WNDCLASSEXW wc = {
        .cbSize = sizeof(wc),
        .lpfnWndProc = tray_wnd_proc,
        .hInstance = hInst,
        .lpszClassName = L"KeyStatsTrayWindow",
    };
    RegisterClassExW(&wc);

    tray->tray_hwnd = CreateWindowExW(
        0, wc.lpszClassName, L"KeyStatsTray", WS_OVERLAPPED,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, hInst, tray);
    SetEvent(tray->ready_ev);

    if (tray->tray_hwnd) {
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    UnregisterClassW(wc.lpszClassName, hInst);
    return 0;
}

KsTray *ks_tray_start(HWND main_hwnd, const char *language, bool paused, bool auto_start) {
    KsTray *tray = (KsTray *)calloc(1, sizeof(KsTray));
    if (!tray) return NULL;
    tray->main_hwnd = main_hwnd;
    atomic_init(&tray->is_zh, resolve_is_zh(language));
    atomic_init(&tray->paused, paused);
    atomic_init(&tray->auto_start, auto_start);
    tray->ready_ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    tray->thread = CreateThread(NULL, 0, tray_thread_proc, tray, 0, NULL);
    if (!tray->thread) {
        if (tray->ready_ev) CloseHandle(tray->ready_ev);
        free(tray);
        return NULL;
    }
    WaitForSingleObject(tray->ready_ev, 2000);
    return tray;
}

void ks_tray_set_language(KsTray *tray, const char *language) {
    if (!tray) return;
    atomic_store(&tray->is_zh, resolve_is_zh(language));
}

void ks_tray_set_state(KsTray *tray, const char *language, bool paused, bool auto_start) {
    if (!tray) return;
    atomic_store(&tray->is_zh, resolve_is_zh(language));
    atomic_store(&tray->paused, paused);
    atomic_store(&tray->auto_start, auto_start);
}

void ks_tray_stop(KsTray *tray) {
    if (!tray) return;
    if (!tray->quit_sent) {
        tray->quit_sent = true;
        if (tray->tray_hwnd) {
            PostMessageW(tray->tray_hwnd, KS_TRAY_APP_QUIT, 0, 0);
        }
        if (tray->thread) {
            WaitForSingleObject(tray->thread, 2000);
            CloseHandle(tray->thread);
        }
        if (tray->ready_ev) CloseHandle(tray->ready_ev);
        free(tray);
    }
}

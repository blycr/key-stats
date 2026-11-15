#include "keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static _Atomic(KsKeyboardService *) g_global_kbd = NULL;

typedef struct {
    DWORD vk_code;
    DWORD scan_code;
    DWORD flags;
    DWORD time;
    ULONG_PTR extra_info;
} KS_KBDLLHOOKSTRUCT;

static void set_active_bit(KsKeyboardService *svc, int vk) {
    int idx = vk >> 6;
    uint64_t bit = 1ULL << (unsigned int)(vk & 63);
    atomic_fetch_or(&svc->active_mask[idx], bit);
}

static void clear_active_bit(KsKeyboardService *svc, int vk) {
    int idx = vk >> 6;
    uint64_t bit = 1ULL << (unsigned int)(vk & 63);
    atomic_fetch_and(&svc->active_mask[idx], ~bit);
}

static void notify_ui(KsKeyboardService *svc) {
    HWND hwnd = svc->notify_hwnd;
    if (!hwnd) return;
    bool expected = false;
    if (atomic_compare_exchange_strong(&svc->notify_pending, &expected, true)) {
        PostMessageW(hwnd, KS_WM_KEYS_DIRTY, 0, 0);
    }
}

void ks_keyboard_record_visual_down(KsKeyboardService *svc, int vk, int64_t now_ms) {
    if (!svc || vk <= 0 || vk >= 256) return;
    atomic_store(&svc->key_down[vk], true);
    atomic_store(&svc->key_down_at[vk], now_ms);
    set_active_bit(svc, vk);
    notify_ui(svc);
}

void ks_keyboard_record_visual_up(KsKeyboardService *svc, int vk) {
    if (!svc || vk <= 0 || vk >= 256) return;
    atomic_store(&svc->key_down[vk], false);
    notify_ui(svc);
}

static bool os_is_vk_physically_down(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static int ctz64(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(x);
#else
    unsigned long idx = 0;
    _BitScanForward64(&idx, x);
    return (int)idx;
#endif
}

int ks_keyboard_active_keys_at(KsKeyboardService *svc, int64_t now_ms, KsIsPhysicallyDownFn phys_fn,
                               char out_names[][KS_KEY_NAME_CAP], int max_names, bool *out_any_active) {
    if (out_any_active) *out_any_active = false;
    if (!svc) return 0;

    uint64_t combined = atomic_load(&svc->active_mask[0]) |
                        atomic_load(&svc->active_mask[1]) |
                        atomic_load(&svc->active_mask[2]) |
                        atomic_load(&svc->active_mask[3]);
    if (combined == 0) {
        return 0;
    }

    int count = 0;
    bool any = false;
    char tmp[KS_KEY_NAME_CAP];

    for (int word = 0; word < 4; word++) {
        uint64_t mask = atomic_load(&svc->active_mask[word]);
        while (mask != 0) {
            int bit = ctz64(mask);
            mask &= mask - 1;
            int vk = (word << 6) + bit;
            if (vk <= 0 || vk >= 256) continue;

            bool held = atomic_load(&svc->key_down[vk]);
            if (held && phys_fn && !phys_fn(vk)) {
                // Recover from missed WM_KEYUP (e.g. Win+L or UAC)
                atomic_store(&svc->key_down[vk], false);
                held = false;
            }
            if (held) {
                any = true;
                if (out_names && count < max_names) {
                    const char *name = ks_vk_to_name(vk, tmp, sizeof(tmp));
                    snprintf(out_names[count], KS_KEY_NAME_CAP, "%s", name);
                    count++;
                }
                continue;
            }

            int64_t down_at = atomic_load(&svc->key_down_at[vk]);
            if (down_at > 0 && (now_ms - down_at) < KS_MIN_KEY_FLASH_MS) {
                any = true;
                if (out_names && count < max_names) {
                    const char *name = ks_vk_to_name(vk, tmp, sizeof(tmp));
                    snprintf(out_names[count], KS_KEY_NAME_CAP, "%s", name);
                    count++;
                }
                continue;
            }

            // Re-verify against concurrent press arriving right before clear
            clear_active_bit(svc, vk);
            if (atomic_load(&svc->key_down[vk]) || atomic_load(&svc->key_down_at[vk]) != down_at) {
                set_active_bit(svc, vk);
                any = true;
            }
        }
    }

    if (out_any_active) *out_any_active = any;
    return count;
}

int ks_keyboard_active_keys(KsKeyboardService *svc, char out_names[][KS_KEY_NAME_CAP], int max_names,
                            bool *out_has_more) {
    return ks_keyboard_active_keys_at(svc, ks_now_unix_ms(), os_is_vk_physically_down,
                                      out_names, max_names, out_has_more);
}

static void get_active_window_title_cached(char *out, size_t cap) {
    static HWND last_hwnd = NULL;
    static char last_title[KS_APP_NAME_CAP] = "Unknown";

    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        snprintf(out, cap, "Unknown");
        return;
    }
    if (hwnd == last_hwnd) {
        snprintf(out, cap, "%s", last_title);
        return;
    }

    wchar_t wtitle[256];
    int len = GetWindowTextW(hwnd, wtitle, 256);
    last_hwnd = hwnd;
    if (len <= 0) {
        snprintf(last_title, sizeof(last_title), "Unknown");
    } else {
        ks_wide_to_utf8_buf(wtitle, last_title, sizeof(last_title));
        if (last_title[0] == '\0') {
            snprintf(last_title, sizeof(last_title), "Unknown");
        }
    }
    snprintf(out, cap, "%s", last_title);
}

static LRESULT CALLBACK low_level_kbd_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && lParam != 0) {
        const KS_KBDLLHOOKSTRUCT *kbd = (const KS_KBDLLHOOKSTRUCT *)lParam;
        int vk = (int)kbd->vk_code;
        KsKeyboardService *svc = atomic_load(&g_global_kbd);
        if (svc) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                int64_t now_ms = ks_now_unix_ms();
                ks_keyboard_record_visual_down(svc, vk, now_ms);
                if (!atomic_load(&svc->paused) && svc->db) {
                    char app_name[KS_APP_NAME_CAP];
                    get_active_window_title_cached(app_name, sizeof(app_name));
                    ks_db_push_event(svc->db, vk, app_name, now_ms);
                }
            } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                ks_keyboard_record_visual_up(svc, vk);
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static DWORD WINAPI hook_thread_proc(LPVOID param) {
    KsKeyboardService *svc = (KsKeyboardService *)param;
    svc->hook_thread_id = GetCurrentThreadId();

    MSG dummy;
    PeekMessageW(&dummy, NULL, 0, 0, PM_NOREMOVE);

    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, low_level_kbd_proc, GetModuleHandleW(NULL), 0);
    if (svc->hook_ready_ev) {
        SetEvent(svc->hook_ready_ev);
    }
    if (!hook) {
        return 1;
    }

    if (!atomic_load(&svc->stopping)) {
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    UnhookWindowsHookEx(hook);
    return 0;
}

KsKeyboardService *ks_keyboard_create(KsDB *db) {
    KsKeyboardService *svc = (KsKeyboardService *)calloc(1, sizeof(KsKeyboardService));
    if (!svc) return NULL;
    svc->db = db;
    InitializeCriticalSection(&svc->state_lock);
    atomic_init(&svc->stopping, false);
    atomic_init(&svc->paused, false);
    atomic_init(&svc->notify_pending, false);
    for (int i = 0; i < 256; i++) {
        atomic_init(&svc->key_down[i], false);
        atomic_init(&svc->key_down_at[i], 0);
    }
    for (int i = 0; i < 4; i++) {
        atomic_init(&svc->active_mask[i], 0);
    }
    return svc;
}

void ks_keyboard_destroy(KsKeyboardService *svc) {
    if (!svc) return;
    ks_keyboard_stop(svc);
    DeleteCriticalSection(&svc->state_lock);
    free(svc);
}

void ks_keyboard_set_notify_hwnd(KsKeyboardService *svc, HWND hwnd) {
    if (!svc) return;
    svc->notify_hwnd = hwnd;
}

void ks_keyboard_clear_notify_pending(KsKeyboardService *svc) {
    if (!svc) return;
    atomic_store(&svc->notify_pending, false);
}

bool ks_keyboard_start(KsKeyboardService *svc) {
    if (!svc) return false;
    EnterCriticalSection(&svc->state_lock);
    if (svc->active) {
        LeaveCriticalSection(&svc->state_lock);
        return true;
    }
    svc->active = true;
    atomic_store(&svc->stopping, false);
    svc->hook_thread_id = 0;
    svc->hook_ready_ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    atomic_store(&g_global_kbd, svc);

    svc->hook_thread = CreateThread(NULL, 0, hook_thread_proc, svc, 0, NULL);
    if (!svc->hook_thread) {
        if (svc->hook_ready_ev) { CloseHandle(svc->hook_ready_ev); svc->hook_ready_ev = NULL; }
        svc->active = false;
        LeaveCriticalSection(&svc->state_lock);
        return false;
    }
    LeaveCriticalSection(&svc->state_lock);

    if (svc->hook_ready_ev) {
        WaitForSingleObject(svc->hook_ready_ev, 2000);
    }
    return true;
}

void ks_keyboard_stop(KsKeyboardService *svc) {
    if (!svc) return;
    EnterCriticalSection(&svc->state_lock);
    if (!svc->active) {
        LeaveCriticalSection(&svc->state_lock);
        return;
    }
    svc->active = false;
    atomic_store(&svc->stopping, true);
    DWORD tid = svc->hook_thread_id;
    HANDLE th = svc->hook_thread;
    HANDLE ev = svc->hook_ready_ev;
    svc->hook_thread = NULL;
    svc->hook_ready_ev = NULL;
    LeaveCriticalSection(&svc->state_lock);

    if (tid != 0) {
        PostThreadMessageW(tid, WM_QUIT, 0, 0);
    }
    if (th) {
        WaitForSingleObject(th, 2000);
        CloseHandle(th);
    }
    if (ev) {
        CloseHandle(ev);
    }
    KsKeyboardService *expected = svc;
    atomic_compare_exchange_strong(&g_global_kbd, &expected, NULL);
}

void ks_keyboard_set_paused(KsKeyboardService *svc, bool paused) {
    if (!svc) return;
    atomic_store(&svc->paused, paused);
}

bool ks_keyboard_is_paused(const KsKeyboardService *svc) {
    if (!svc) return false;
    return atomic_load(&svc->paused);
}

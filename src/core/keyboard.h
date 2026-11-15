#ifndef KS_KEYBOARD_H
#define KS_KEYBOARD_H

#include "db.h"
#include "stats.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KS_MIN_KEY_FLASH_MS 180
#define KS_WM_KEYS_DIRTY (WM_APP + 20)
#define KS_WM_STATS_DIRTY (WM_APP + 21)

typedef bool (*KsIsPhysicallyDownFn)(int vk);

typedef struct {
    KsDB *db;
    CRITICAL_SECTION state_lock;
    bool active;
    atomic_bool stopping;
    atomic_bool paused;
    DWORD hook_thread_id;
    HANDLE hook_thread;
    HANDLE hook_ready_ev;

    // Lock-free visual key state indexed by Windows VK code (0..255)
    atomic_bool key_down[256];
    atomic_int_fast64_t key_down_at[256];
    atomic_uint_fast64_t active_mask[4];

    // Notification target when visual keys change (coalesced via notify_pending)
    HWND notify_hwnd;
    atomic_bool notify_pending;
} KsKeyboardService;

KsKeyboardService *ks_keyboard_create(KsDB *db);
void ks_keyboard_destroy(KsKeyboardService *svc);

void ks_keyboard_set_notify_hwnd(KsKeyboardService *svc, HWND hwnd);
void ks_keyboard_clear_notify_pending(KsKeyboardService *svc);

bool ks_keyboard_start(KsKeyboardService *svc);
void ks_keyboard_stop(KsKeyboardService *svc);

void ks_keyboard_set_paused(KsKeyboardService *svc, bool paused);
bool ks_keyboard_is_paused(const KsKeyboardService *svc);

// Writes currently active key display names into out_names (each KS_KEY_NAME_CAP).
// Returns count written (0..max_names), and sets *out_has_fading_or_held if timer should keep ticking.
int ks_keyboard_active_keys(KsKeyboardService *svc, char out_names[][KS_KEY_NAME_CAP], int max_names,
                            bool *out_has_more);

// Deterministic helpers exposed for unit testing.
void ks_keyboard_record_visual_down(KsKeyboardService *svc, int vk, int64_t now_ms);
void ks_keyboard_record_visual_up(KsKeyboardService *svc, int vk);
int ks_keyboard_active_keys_at(KsKeyboardService *svc, int64_t now_ms, KsIsPhysicallyDownFn phys_fn,
                               char out_names[][KS_KEY_NAME_CAP], int max_names, bool *out_any_active);

#ifdef __cplusplus
}
#endif

#endif // KS_KEYBOARD_H

#ifndef KS_WIN32_UTIL_H
#define KS_WIN32_UTIL_H

#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KS_MAIN_WINDOW_CLASS L"KeyStatsMainWindow"
#define KS_SINGLE_INSTANCE_MUTEX L"Local\\KeyStatsSingleInstanceMutex"
#define KS_WM_SHOW_EXISTING (WM_APP + 10)

// Converts UTF-8 string to newly allocated UTF-16 string (caller frees with free()).
// Returns NULL on allocation failure.
wchar_t *ks_utf8_to_wide_alloc(const char *utf8);

// Converts UTF-8 string into a fixed wide buffer; always NUL-terminates if cap > 0.
int ks_utf8_to_wide_buf(const char *utf8, wchar_t *out, int cap);

// Converts UTF-16 string to newly allocated UTF-8 string (caller frees with free()).
char *ks_wide_to_utf8_alloc(const wchar_t *wide);

// Converts UTF-16 string into a fixed UTF-8 buffer; always NUL-terminates if cap > 0.
int ks_wide_to_utf8_buf(const wchar_t *wide, char *out, int cap);

// Recursively creates directories for a wide directory path (like os.MkdirAll).
bool ks_mkdir_recursive_w(const wchar_t *dir_path);

// Ensures parent directory of a file path exists.
bool ks_ensure_parent_dir_w(const wchar_t *file_path);

// Returns current UTC Unix epoch in milliseconds.
int64_t ks_now_unix_ms(void);

// Returns monotonic clock in milliseconds (GetTickCount64).
uint64_t ks_monotonic_ms(void);

// Single instance enforcement. Returns mutex handle if acquired; out_acquired=false
// if another instance already holds it.
HANDLE ks_acquire_single_instance(const wchar_t *mutex_name, bool *out_acquired);

// Finds existing KeyStats window and asks it to restore/focus.
bool ks_focus_existing_window(void);

#ifdef __cplusplus
}
#endif

#endif // KS_WIN32_UTIL_H

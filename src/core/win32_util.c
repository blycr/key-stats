#include "win32_util.h"
#include <stdlib.h>
#include <string.h>

wchar_t *ks_utf8_to_wide_alloc(const char *utf8) {
    if (!utf8) return NULL;
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len <= 0) return NULL;
    wchar_t *buf = (wchar_t *)malloc((size_t)len * sizeof(wchar_t));
    if (!buf) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf, len) <= 0) {
        free(buf);
        return NULL;
    }
    return buf;
}

int ks_utf8_to_wide_buf(const char *utf8, wchar_t *out, int cap) {
    if (!out || cap <= 0) return 0;
    out[0] = L'\0';
    if (!utf8 || utf8[0] == '\0') return 0;
    int written = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, cap);
    if (written <= 0) {
        out[cap - 1] = L'\0';
        return 0;
    }
    return written - 1;
}

char *ks_wide_to_utf8_alloc(const wchar_t *wide) {
    if (!wide) return NULL;
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char *buf = (char *)malloc((size_t)len);
    if (!buf) return NULL;
    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, buf, len, NULL, NULL) <= 0) {
        free(buf);
        return NULL;
    }
    return buf;
}

int ks_wide_to_utf8_buf(const wchar_t *wide, char *out, int cap) {
    if (!out || cap <= 0) return 0;
    out[0] = '\0';
    if (!wide || wide[0] == L'\0') return 0;
    int written = WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, cap, NULL, NULL);
    if (written <= 0) {
        out[cap - 1] = '\0';
        return 0;
    }
    return written - 1;
}

bool ks_mkdir_recursive_w(const wchar_t *dir_path) {
    if (!dir_path || dir_path[0] == L'\0') return false;

    size_t len = wcslen(dir_path);
    wchar_t *tmp = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (!tmp) return false;
    memcpy(tmp, dir_path, (len + 1) * sizeof(wchar_t));

    // Normalize slashes to backslash and trim trailing slash (except drive root like C:\).
    for (size_t i = 0; i < len; i++) {
        if (tmp[i] == L'/') tmp[i] = L'\\';
    }
    while (len > 3 && tmp[len - 1] == L'\\') {
        tmp[--len] = L'\0';
    }

    for (size_t i = 0; i < len; i++) {
        if (tmp[i] == L'\\') {
            // Skip UNC prefix or drive root "X:\"
            if (i == 0 || (i == 2 && tmp[1] == L':')) continue;
            tmp[i] = L'\0';
            DWORD attrs = GetFileAttributesW(tmp);
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                if (!CreateDirectoryW(tmp, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                    free(tmp);
                    return false;
                }
            }
            tmp[i] = L'\\';
        }
    }

    DWORD attrs = GetFileAttributesW(tmp);
    bool ok = true;
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryW(tmp, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            ok = false;
        }
    } else if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        ok = false;
    }

    free(tmp);
    return ok;
}

bool ks_ensure_parent_dir_w(const wchar_t *file_path) {
    if (!file_path) return false;
    size_t len = wcslen(file_path);
    wchar_t *copy = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (!copy) return false;
    memcpy(copy, file_path, (len + 1) * sizeof(wchar_t));

    wchar_t *last_slash = wcsrchr(copy, L'\\');
    wchar_t *last_fslash = wcsrchr(copy, L'/');
    if (last_fslash && (!last_slash || last_fslash > last_slash)) {
        last_slash = last_fslash;
    }
    bool ok = true;
    if (last_slash) {
        *last_slash = L'\0';
        ok = ks_mkdir_recursive_w(copy);
    }
    free(copy);
    return ok;
}

int64_t ks_now_unix_ms(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER ui;
    ui.LowPart = ft.dwLowDateTime;
    ui.HighPart = ft.dwHighDateTime;
    // Windows epoch (1601-01-01) to Unix epoch (1970-01-01) in 100ns ticks.
    const uint64_t epoch_diff_100ns = 116444736000000000ULL;
    if (ui.QuadPart < epoch_diff_100ns) return 0;
    return (int64_t)((ui.QuadPart - epoch_diff_100ns) / 10000ULL);
}

uint64_t ks_monotonic_ms(void) {
    return GetTickCount64();
}

HANDLE ks_acquire_single_instance(const wchar_t *mutex_name, bool *out_acquired) {
    if (out_acquired) *out_acquired = true;
    HANDLE h = CreateMutexW(NULL, FALSE, mutex_name);
    if (!h) return NULL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(h);
        if (out_acquired) *out_acquired = false;
        return NULL;
    }
    return h;
}

bool ks_focus_existing_window(void) {
    HWND hwnd = FindWindowW(KS_MAIN_WINDOW_CLASS, NULL);
    if (!hwnd) {
        // Fallback: legacy window class if older version is running
        hwnd = FindWindowW(L"wailsWindow", NULL);
    }
    if (!hwnd) return false;
    PostMessageW(hwnd, KS_WM_SHOW_EXISTING, 0, 0);
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
    return true;
}

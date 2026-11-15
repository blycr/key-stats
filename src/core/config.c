#include "config.h"
#include <shlobj.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void str_trim_inplace(char *s) {
    if (!s) return;
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static bool parse_bool_str(const char *s) {
    if (!s) return false;
    char buf[32];
    snprintf(buf, sizeof(buf), "%s", s);
    str_trim_inplace(buf);
    for (size_t i = 0; buf[i]; i++) {
        buf[i] = (char)tolower((unsigned char)buf[i]);
    }
    return strcmp(buf, "true") == 0 || strcmp(buf, "1") == 0 || strcmp(buf, "yes") == 0;
}

static void sanitize_single_line(const char *in, char *out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!in) return;
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < cap; i++) {
        if (in[i] == '\r' || in[i] == '\n') continue;
        out[j++] = in[i];
    }
    out[j] = '\0';
    str_trim_inplace(out);
}

void ks_normalize_theme(const char *in, char *out, size_t cap) {
    char buf[32] = {0};
    if (in) snprintf(buf, sizeof(buf), "%s", in);
    str_trim_inplace(buf);
    for (size_t i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    if (strcmp(buf, "light") == 0) snprintf(out, cap, "light");
    else if (strcmp(buf, "auto") == 0) snprintf(out, cap, "auto");
    else snprintf(out, cap, "dark");
}

void ks_normalize_color_scheme(const char *in, char *out, size_t cap) {
    char buf[32] = {0};
    if (in) snprintf(buf, sizeof(buf), "%s", in);
    str_trim_inplace(buf);
    for (size_t i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    if (strcmp(buf, "ocean") == 0) snprintf(out, cap, "ocean");
    else if (strcmp(buf, "emerald") == 0) snprintf(out, cap, "emerald");
    else if (strcmp(buf, "amber") == 0) snprintf(out, cap, "amber");
    else if (strcmp(buf, "rose") == 0) snprintf(out, cap, "rose");
    else snprintf(out, cap, "indigo");
}

void ks_normalize_language(const char *in, char *out, size_t cap) {
    char buf[32] = {0};
    if (in) snprintf(buf, sizeof(buf), "%s", in);
    str_trim_inplace(buf);
    for (size_t i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    if (strcmp(buf, "zh") == 0 || strcmp(buf, "zh-cn") == 0 ||
        strcmp(buf, "zh_cn") == 0 || strcmp(buf, "zh-hans") == 0) {
        snprintf(out, cap, "zh");
    } else if (strcmp(buf, "en") == 0 || strcmp(buf, "en-us") == 0 ||
               strcmp(buf, "en_us") == 0) {
        snprintf(out, cap, "en");
    } else {
        snprintf(out, cap, "auto");
    }
}

void ks_config_default(KsConfig *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->window_x = 0;
    out->window_y = 0;
    out->has_window_pos = false;
    out->window_width = KS_DEFAULT_WINDOW_WIDTH;
    out->window_height = KS_DEFAULT_WINDOW_HEIGHT;
    snprintf(out->theme, sizeof(out->theme), "dark");
    snprintf(out->color_scheme, sizeof(out->color_scheme), "indigo");
    snprintf(out->font_family, sizeof(out->font_family), "%s", KS_DEFAULT_FONT_FAMILY);
    snprintf(out->language, sizeof(out->language), "auto");
    out->start_minimized = false;
    out->auto_start = false;
    out->portable_mode = false;
}

static bool file_exists_w(const wchar_t *path) {
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool env_explicit_portable_false(const wchar_t *path) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return false;
    bool explicit_false = false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        str_trim_inplace(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = line;
        char *v = eq + 1;
        str_trim_inplace(k);
        str_trim_inplace(v);
        if (strcmp(k, "PORTABLE_MODE") == 0 && !parse_bool_str(v)) {
            explicit_false = true;
        }
    }
    fclose(f);
    return explicit_false;
}

void ks_config_resolve_location(const wchar_t *exe_path, const wchar_t *roaming_base_dir,
                                wchar_t *out_env_path, size_t env_cap,
                                wchar_t *out_data_dir, size_t dir_cap,
                                bool *out_portable) {
    if (exe_path && exe_path[0]) {
        wchar_t exe_dir[MAX_PATH];
        _snwprintf(exe_dir, MAX_PATH, L"%ls", exe_path);
        exe_dir[MAX_PATH - 1] = L'\0';
        wchar_t *slash = wcsrchr(exe_dir, L'\\');
        wchar_t *fslash = wcsrchr(exe_dir, L'/');
        if (fslash && (!slash || fslash > slash)) slash = fslash;
        if (slash) {
            *slash = L'\0';
            wchar_t portable_env[MAX_PATH];
            _snwprintf(portable_env, MAX_PATH, L"%ls\\.env", exe_dir);
            portable_env[MAX_PATH - 1] = L'\0';
            if (file_exists_w(portable_env) && !env_explicit_portable_false(portable_env)) {
                if (out_env_path) _snwprintf(out_env_path, env_cap, L"%ls", portable_env);
                if (out_data_dir) _snwprintf(out_data_dir, dir_cap, L"%ls", exe_dir);
                if (out_portable) *out_portable = true;
                return;
            }
        }
    }

    wchar_t roaming_dir[MAX_PATH] = L".";
    if (roaming_base_dir && roaming_base_dir[0]) {
        _snwprintf(roaming_dir, MAX_PATH, L"%ls\\%ls", roaming_base_dir, KS_APP_DIR_NAME);
        roaming_dir[MAX_PATH - 1] = L'\0';
    }
    if (out_env_path) _snwprintf(out_env_path, env_cap, L"%ls\\.env", roaming_dir);
    if (out_data_dir) _snwprintf(out_data_dir, dir_cap, L"%ls", roaming_dir);
    if (out_portable) *out_portable = false;
}

bool ks_config_current_location(wchar_t *out_env_path, size_t env_cap,
                                wchar_t *out_data_dir, size_t dir_cap,
                                bool *out_portable) {
    wchar_t exe_path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);

    wchar_t appdata[MAX_PATH] = {0};
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata))) {
        GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    }

    ks_config_resolve_location(exe_path, appdata, out_env_path, env_cap,
                               out_data_dir, dir_cap, out_portable);
    return true;
}

bool ks_config_load_from_path(const wchar_t *path, bool portable, KsConfig *out) {
    if (!out) return false;
    ks_config_default(out);
    out->portable_mode = portable;

    if (!path || !file_exists_w(path)) {
        return true; // defaults are valid when .env does not yet exist
    }

    FILE *f = _wfopen(path, L"rb");
    if (!f) return false;

    char line[512];
    bool seen_x = false, seen_y = false;
    int px = 0, py = 0;
    while (fgets(line, sizeof(line), f)) {
        str_trim_inplace(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = line;
        char *v = eq + 1;
        str_trim_inplace(k);
        str_trim_inplace(v);

        if (strcmp(k, "WINDOW_X") == 0) {
            px = atoi(v);
            seen_x = true;
        } else if (strcmp(k, "WINDOW_Y") == 0) {
            py = atoi(v);
            seen_y = true;
        } else if (strcmp(k, "WINDOW_WIDTH") == 0) {
            int val = atoi(v);
            if (val >= KS_MIN_WINDOW_WIDTH) out->window_width = val;
        } else if (strcmp(k, "WINDOW_HEIGHT") == 0) {
            int val = atoi(v);
            if (val >= KS_MIN_WINDOW_HEIGHT) out->window_height = val;
        } else if (strcmp(k, "THEME") == 0) {
            ks_normalize_theme(v, out->theme, sizeof(out->theme));
        } else if (strcmp(k, "COLOR_SCHEME") == 0) {
            ks_normalize_color_scheme(v, out->color_scheme, sizeof(out->color_scheme));
        } else if (strcmp(k, "FONT_FAMILY") == 0) {
            char tmp[128];
            sanitize_single_line(v, tmp, sizeof(tmp));
            snprintf(out->font_family, sizeof(out->font_family), "%s", tmp);
        } else if (strcmp(k, "LANGUAGE") == 0) {
            ks_normalize_language(v, out->language, sizeof(out->language));
        } else if (strcmp(k, "START_MINIMIZED") == 0) {
            out->start_minimized = parse_bool_str(v);
        } else if (strcmp(k, "AUTO_START") == 0) {
            out->auto_start = parse_bool_str(v);
        } else if (strcmp(k, "PORTABLE_MODE") == 0 && portable) {
            out->portable_mode = parse_bool_str(v);
        }
    }
    if (seen_x && seen_y) {
        out->window_x = px;
        out->window_y = py;
        out->has_window_pos = true;
    }
    fclose(f);
    return true;
}

bool ks_config_save_to_path(const wchar_t *path, const KsConfig *cfg, bool portable) {
    if (!path || !cfg) return false;
    if (!ks_ensure_parent_dir_w(path)) return false;

    char theme[16], scheme[32], lang[16], font[128];
    ks_normalize_theme(cfg->theme, theme, sizeof(theme));
    ks_normalize_color_scheme(cfg->color_scheme, scheme, sizeof(scheme));
    ks_normalize_language(cfg->language, lang, sizeof(lang));
    sanitize_single_line(cfg->font_family, font, sizeof(font));

    FILE *f = _wfopen(path, L"wb");
    if (!f) return false;

    const char *portable_line = portable ? "PORTABLE_MODE=true" : "# PORTABLE_MODE=false";
    fprintf(f,
        "# KeyStats Configuration\n"
        "# ======================\n"
        "# Edit values below to customize.\n\n"
        "# Window Geometry (last known position & dimensions)\n");
    if (cfg->has_window_pos) {
        fprintf(f, "WINDOW_X=%d\nWINDOW_Y=%d\n", cfg->window_x, cfg->window_y);
    }
    fprintf(f,
        "WINDOW_WIDTH=%d\n"
        "WINDOW_HEIGHT=%d\n\n"
        "# Appearance — dark | light | auto\n"
        "THEME=%s\n"
        "# Color Scheme — indigo | ocean | emerald | amber | rose\n"
        "COLOR_SCHEME=%s\n\n"
        "# Font Family\n"
        "FONT_FAMILY=%s\n\n"
        "# Language — auto | en | zh\n"
        "LANGUAGE=%s\n\n"
        "# Behavior\n"
        "START_MINIMIZED=%s\n"
        "AUTO_START=%s\n\n"
        "# Data Storage (detected automatically by .env location)\n"
        "%s\n"
        "# To enable portable mode: copy this .env file next to key-stats.exe\n",
        cfg->window_width < KS_MIN_WINDOW_WIDTH ? KS_DEFAULT_WINDOW_WIDTH : cfg->window_width,
        cfg->window_height < KS_MIN_WINDOW_HEIGHT ? KS_DEFAULT_WINDOW_HEIGHT : cfg->window_height,
        theme, scheme, font, lang,
        cfg->start_minimized ? "true" : "false",
        cfg->auto_start ? "true" : "false",
        portable_line);

    bool ok = ferror(f) == 0;
    fclose(f);
    return ok;
}

void ks_config_manager_init(KsConfigManager *mgr) {
    wchar_t env_path[MAX_PATH] = {0};
    wchar_t data_dir[MAX_PATH] = {0};
    bool portable = false;
    ks_config_current_location(env_path, MAX_PATH, data_dir, MAX_PATH, &portable);
    ks_config_manager_init_custom(mgr, env_path, data_dir, portable);
}

void ks_config_manager_init_custom(KsConfigManager *mgr, const wchar_t *env_path,
                                   const wchar_t *data_dir, bool portable) {
    memset(mgr, 0, sizeof(*mgr));
    InitializeSRWLock(&mgr->lock);
    if (env_path) _snwprintf(mgr->env_path, MAX_PATH, L"%ls", env_path);
    if (data_dir) _snwprintf(mgr->data_dir, MAX_PATH, L"%ls", data_dir);
    ks_config_load_from_path(mgr->env_path, portable, &mgr->cfg);
}

KsConfig ks_config_manager_get(KsConfigManager *mgr) {
    AcquireSRWLockShared(&mgr->lock);
    KsConfig copy = mgr->cfg;
    ReleaseSRWLockShared(&mgr->lock);
    return copy;
}

bool ks_config_manager_update(KsConfigManager *mgr, const KsConfig *next,
                              bool *out_autostart_changed, bool *out_lang_changed) {
    if (!mgr || !next) return false;
    AcquireSRWLockExclusive(&mgr->lock);

    KsConfig updated = mgr->cfg;
    bool autostart_changed = (next->auto_start != updated.auto_start);
    char norm_theme[16], norm_scheme[32], norm_lang[16], clean_font[128];
    ks_normalize_theme(next->theme, norm_theme, sizeof(norm_theme));
    ks_normalize_color_scheme(next->color_scheme, norm_scheme, sizeof(norm_scheme));
    ks_normalize_language(next->language, norm_lang, sizeof(norm_lang));
    sanitize_single_line(next->font_family, clean_font, sizeof(clean_font));

    bool lang_changed = (strcmp(norm_lang, updated.language) != 0);

    if (next->window_width >= KS_MIN_WINDOW_WIDTH) updated.window_width = next->window_width;
    if (next->window_height >= KS_MIN_WINDOW_HEIGHT) updated.window_height = next->window_height;
    if (next->has_window_pos) {
        updated.window_x = next->window_x;
        updated.window_y = next->window_y;
        updated.has_window_pos = true;
    }
    snprintf(updated.theme, sizeof(updated.theme), "%s", norm_theme);
    snprintf(updated.color_scheme, sizeof(updated.color_scheme), "%s", norm_scheme);
    snprintf(updated.language, sizeof(updated.language), "%s", norm_lang);
    snprintf(updated.font_family, sizeof(updated.font_family), "%s", clean_font);
    updated.start_minimized = next->start_minimized;
    updated.auto_start = next->auto_start;

    bool ok = ks_config_save_to_path(mgr->env_path, &updated, updated.portable_mode);
    if (ok) {
        mgr->cfg = updated;
        if (out_autostart_changed) *out_autostart_changed = autostart_changed;
        if (out_lang_changed) *out_lang_changed = lang_changed;
    }

    ReleaseSRWLockExclusive(&mgr->lock);
    return ok;
}

bool ks_config_manager_set_window_size(KsConfigManager *mgr, int width, int height) {
    if (!mgr || width < KS_MIN_WINDOW_WIDTH || height < KS_MIN_WINDOW_HEIGHT) return false;
    AcquireSRWLockExclusive(&mgr->lock);
    if (mgr->cfg.window_width == width && mgr->cfg.window_height == height) {
        ReleaseSRWLockExclusive(&mgr->lock);
        return true;
    }
    KsConfig updated = mgr->cfg;
    updated.window_width = width;
    updated.window_height = height;
    bool ok = ks_config_save_to_path(mgr->env_path, &updated, updated.portable_mode);
    if (ok) {
        mgr->cfg = updated;
    }
    ReleaseSRWLockExclusive(&mgr->lock);
    return ok;
}

bool ks_config_manager_set_window_rect(KsConfigManager *mgr, int x, int y, int width, int height, bool has_pos) {
    if (!mgr || width < KS_MIN_WINDOW_WIDTH || height < KS_MIN_WINDOW_HEIGHT) return false;
    AcquireSRWLockExclusive(&mgr->lock);
    if (mgr->cfg.window_width == width && mgr->cfg.window_height == height &&
        mgr->cfg.has_window_pos == has_pos &&
        (!has_pos || (mgr->cfg.window_x == x && mgr->cfg.window_y == y))) {
        ReleaseSRWLockExclusive(&mgr->lock);
        return true;
    }
    KsConfig updated = mgr->cfg;
    updated.window_width = width;
    updated.window_height = height;
    updated.has_window_pos = has_pos;
    if (has_pos) {
        updated.window_x = x;
        updated.window_y = y;
    }
    bool ok = ks_config_save_to_path(mgr->env_path, &updated, updated.portable_mode);
    if (ok) {
        mgr->cfg = updated;
    }
    ReleaseSRWLockExclusive(&mgr->lock);
    return ok;
}

bool ks_config_manager_reset_defaults(KsConfigManager *mgr) {
    if (!mgr) return false;
    AcquireSRWLockExclusive(&mgr->lock);
    KsConfig updated;
    ks_config_default(&updated);
    updated.portable_mode = mgr->cfg.portable_mode;
    bool ok = ks_config_save_to_path(mgr->env_path, &updated, updated.portable_mode);
    if (ok) {
        mgr->cfg = updated;
    }
    ReleaseSRWLockExclusive(&mgr->lock);
    return ok;
}

bool ks_apply_autostart(bool enabled) {
    const wchar_t *run_key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    HKEY hkey = NULL;
    LONG res = RegOpenKeyExW(HKEY_CURRENT_USER, run_key, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hkey);
    if (res != ERROR_SUCCESS) return false;

    bool ok = false;
    if (enabled) {
        wchar_t exe[MAX_PATH] = {0};
        DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            wchar_t quoted[MAX_PATH + 4];
            _snwprintf(quoted, MAX_PATH + 4, L"\"%ls\"", exe);
            DWORD bytes = (DWORD)((wcslen(quoted) + 1) * sizeof(wchar_t));
            ok = (RegSetValueExW(hkey, L"KeyStats", 0, REG_SZ, (const BYTE *)quoted, bytes) == ERROR_SUCCESS);
        }
    } else {
        LONG d = RegDeleteValueW(hkey, L"KeyStats");
        ok = (d == ERROR_SUCCESS || d == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(hkey);
    return ok;
}

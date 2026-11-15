#ifndef KS_CONFIG_H
#define KS_CONFIG_H

#include "win32_util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KS_APP_DIR_NAME L"key-stats"
#define KS_MIN_WINDOW_WIDTH 780
#define KS_MIN_WINDOW_HEIGHT 540
#define KS_DEFAULT_WINDOW_WIDTH 1120
#define KS_DEFAULT_WINDOW_HEIGHT 680
#define KS_DEFAULT_FONT_FAMILY "JetBrains Mono"

typedef struct {
    int window_x;
    int window_y;
    bool has_window_pos;
    int window_width;
    int window_height;
    char theme[16];          // "dark" | "light" | "auto"
    char color_scheme[32];   // "indigo" | "ocean" | "emerald" | "amber" | "rose"
    char font_family[128];   // sanitized single-line UTF-8
    char language[16];       // "auto" | "en" | "zh"
    bool start_minimized;
    bool auto_start;
    bool portable_mode;
} KsConfig;

typedef struct {
    SRWLOCK lock;
    KsConfig cfg;
    wchar_t env_path[MAX_PATH];
    wchar_t data_dir[MAX_PATH];
} KsConfigManager;

void ks_config_default(KsConfig *out);

// Resolves .env path and data directory given executable path and roaming AppData dir.
void ks_config_resolve_location(const wchar_t *exe_path, const wchar_t *roaming_base_dir,
                                wchar_t *out_env_path, size_t env_cap,
                                wchar_t *out_data_dir, size_t dir_cap,
                                bool *out_portable);

// Resolves location for the running executable and %APPDATA%.
bool ks_config_current_location(wchar_t *out_env_path, size_t env_cap,
                                wchar_t *out_data_dir, size_t dir_cap,
                                bool *out_portable);

bool ks_config_load_from_path(const wchar_t *path, bool portable, KsConfig *out);
bool ks_config_save_to_path(const wchar_t *path, const KsConfig *cfg, bool portable);

void ks_config_manager_init(KsConfigManager *mgr);
void ks_config_manager_init_custom(KsConfigManager *mgr, const wchar_t *env_path,
                                   const wchar_t *data_dir, bool portable);
KsConfig ks_config_manager_get(KsConfigManager *mgr);
bool ks_config_manager_update(KsConfigManager *mgr, const KsConfig *next,
                              bool *out_autostart_changed, bool *out_lang_changed);
bool ks_config_manager_set_window_size(KsConfigManager *mgr, int width, int height);
bool ks_config_manager_set_window_rect(KsConfigManager *mgr, int x, int y, int width, int height, bool has_pos);
bool ks_config_manager_reset_defaults(KsConfigManager *mgr);

// Registers or removes KeyStats under HKCU\Software\Microsoft\Windows\CurrentVersion\Run.
bool ks_apply_autostart(bool enabled);

// Normalizers exposed for tests and callers.
void ks_normalize_theme(const char *in, char *out, size_t cap);
void ks_normalize_color_scheme(const char *in, char *out, size_t cap);
void ks_normalize_language(const char *in, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif // KS_CONFIG_H

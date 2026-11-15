#include "test_runner.h"
#include "../src/core/win32_util.h"
#include "../src/core/config.h"
#include "../src/core/db.h"
#include "../src/core/stats.h"
#include "../src/core/keyboard.h"
#include <direct.h>

static void make_temp_dir_w(const wchar_t *suffix, wchar_t *out, size_t cap) {
    wchar_t base[MAX_PATH] = {0};
    GetTempPathW(MAX_PATH, base);
    _snwprintf(out, cap, L"%lskeystats_test_%ls_%lu", base, suffix, (unsigned long)GetTickCount());
    out[cap - 1] = L'\0';
    ks_mkdir_recursive_w(out);
}

static bool test_config_resolve_and_roundtrip(void) {
    wchar_t dir[MAX_PATH];
    make_temp_dir_w(L"cfg", dir, MAX_PATH);

    wchar_t exe_dir[MAX_PATH], exe_path[MAX_PATH], roaming[MAX_PATH];
    _snwprintf(exe_dir, MAX_PATH, L"%ls\\app", dir);
    _snwprintf(exe_path, MAX_PATH, L"%ls\\key-stats.exe", exe_dir);
    _snwprintf(roaming, MAX_PATH, L"%ls\\roaming", dir);
    ks_mkdir_recursive_w(exe_dir);
    ks_mkdir_recursive_w(roaming);

    wchar_t env_out[MAX_PATH], data_out[MAX_PATH];
    bool portable = false;

    // 1. Without portable .env -> falls back to %APPDATA%\key-stats
    ks_config_resolve_location(exe_path, roaming, env_out, MAX_PATH, data_out, MAX_PATH, &portable);
    KS_ASSERT_TRUE(!portable);

    // 2. Portable .env next to exe -> activates portable mode
    wchar_t p_env[MAX_PATH];
    _snwprintf(p_env, MAX_PATH, L"%ls\\.env", exe_dir);
    FILE *f = _wfopen(p_env, L"wb");
    KS_ASSERT_TRUE(f != NULL);
    fprintf(f, "THEME=light\nLANGUAGE=zh\nFONT_FAMILY=Cascadia Code\nWINDOW_WIDTH=1400\n");
    fclose(f);

    ks_config_resolve_location(exe_path, roaming, env_out, MAX_PATH, data_out, MAX_PATH, &portable);
    KS_ASSERT_TRUE(portable);

    KsConfig loaded;
    KS_ASSERT_TRUE(ks_config_load_from_path(p_env, true, &loaded));
    KS_ASSERT_EQ_STR(loaded.theme, "light");
    KS_ASSERT_EQ_STR(loaded.language, "zh");
    KS_ASSERT_EQ_STR(loaded.font_family, "Cascadia Code");
    KS_ASSERT_EQ_INT(loaded.window_width, 1400);

    // 3. Explicit PORTABLE_MODE=false disables portable mode
    f = _wfopen(p_env, L"wb");
    KS_ASSERT_TRUE(f != NULL);
    fprintf(f, "PORTABLE_MODE=false\n");
    fclose(f);
    ks_config_resolve_location(exe_path, roaming, env_out, MAX_PATH, data_out, MAX_PATH, &portable);
    KS_ASSERT_TRUE(!portable);

    // 4. Round-trip save & load via KsConfigManager
    wchar_t custom_env[MAX_PATH];
    _snwprintf(custom_env, MAX_PATH, L"%ls\\saved.env", dir);
    KsConfigManager mgr;
    ks_config_manager_init_custom(&mgr, custom_env, dir, false);
    KsConfig next = ks_config_manager_get(&mgr);
    snprintf(next.theme, sizeof(next.theme), "AUTO");
    snprintf(next.color_scheme, sizeof(next.color_scheme), "OCEAN");
    snprintf(next.language, sizeof(next.language), "zh-CN");
    snprintf(next.font_family, sizeof(next.font_family), "Fira Code\nEvil");
    next.window_width = 960;
    next.window_height = 720;
    next.window_x = 120;
    next.window_y = 84;
    next.has_window_pos = true;
    next.start_minimized = true;
    bool as_changed = false, lang_changed = false;
    KS_ASSERT_TRUE(ks_config_manager_update(&mgr, &next, &as_changed, &lang_changed));
    KS_ASSERT_TRUE(lang_changed);

    KsConfig reloaded;
    KS_ASSERT_TRUE(ks_config_load_from_path(custom_env, false, &reloaded));
    KS_ASSERT_EQ_STR(reloaded.theme, "auto");
    KS_ASSERT_EQ_STR(reloaded.color_scheme, "ocean");
    KS_ASSERT_EQ_STR(reloaded.language, "zh");
    KS_ASSERT_EQ_STR(reloaded.font_family, "Fira CodeEvil");
    KS_ASSERT_EQ_INT(reloaded.window_width, 960);
    KS_ASSERT_EQ_INT(reloaded.window_height, 720);
    KS_ASSERT_EQ_INT(reloaded.window_x, 120);
    KS_ASSERT_EQ_INT(reloaded.window_y, 84);
    KS_ASSERT_TRUE(reloaded.has_window_pos);
    KS_ASSERT_TRUE(reloaded.start_minimized);

    // 4b. Test 4:3 window rect helper & System Default font reset
    KS_ASSERT_TRUE(ks_config_manager_set_window_rect(&mgr, 42, 64, 800, 600, true));
    KS_ASSERT_TRUE(ks_config_load_from_path(custom_env, false, &reloaded));
    KS_ASSERT_EQ_INT(reloaded.window_width, 800);
    KS_ASSERT_EQ_INT(reloaded.window_height, 600);
    KS_ASSERT_EQ_INT(reloaded.window_x, 42);
    KS_ASSERT_EQ_INT(reloaded.window_y, 64);

    // 4c. Reset defaults clears custom layout/scheme cleanly
    KS_ASSERT_TRUE(ks_config_manager_reset_defaults(&mgr));
    KS_ASSERT_TRUE(ks_config_load_from_path(custom_env, false, &reloaded));
    KS_ASSERT_EQ_STR(reloaded.theme, "dark");
    KS_ASSERT_EQ_STR(reloaded.color_scheme, "indigo");
    KS_ASSERT_EQ_STR(reloaded.font_family, "JetBrains Mono");
    KS_ASSERT_EQ_INT(reloaded.window_width, KS_DEFAULT_WINDOW_WIDTH);
    KS_ASSERT_EQ_INT(reloaded.window_height, KS_DEFAULT_WINDOW_HEIGHT);
    KS_ASSERT_TRUE(!reloaded.has_window_pos);

    // 4d. Empty font_family (System Default) round-trips as ""
    next = ks_config_manager_get(&mgr);
    next.font_family[0] = '\0';
    KS_ASSERT_TRUE(ks_config_manager_update(&mgr, &next, &as_changed, &lang_changed));
    KS_ASSERT_TRUE(ks_config_load_from_path(custom_env, false, &reloaded));
    KS_ASSERT_EQ_STR(reloaded.font_family, "");
    return true;
}

static bool test_db_rollup_backfill_flush_reset(void) {
    wchar_t dir[MAX_PATH];
    make_temp_dir_w(L"db", dir, MAX_PATH);

    // Pre-seed legacy DB containing only key_events + dropped index
    wchar_t db_w[MAX_PATH];
    _snwprintf(db_w, MAX_PATH, L"%ls\\data.db", dir);
    char *db_u8 = ks_wide_to_utf8_alloc(db_w);
    KS_ASSERT_TRUE(db_u8 != NULL);

    sqlite3 *legacy = NULL;
    KS_ASSERT_EQ_INT(sqlite3_open(db_u8, &legacy), SQLITE_OK);
    free(db_u8);
    const char *seed_sql =
        "CREATE TABLE key_events (id INTEGER PRIMARY KEY AUTOINCREMENT, key_code INTEGER NOT NULL, app_name TEXT NOT NULL, timestamp INTEGER NOT NULL);"
        "CREATE INDEX idx_key_events_key_code ON key_events (key_code);";
    KS_ASSERT_EQ_INT(sqlite3_exec(legacy, seed_sql, NULL, NULL, NULL), SQLITE_OK);

    int64_t ts_base = 1794736920000LL; // 2026-11-15 10:02:00 UTC in ms
    for (int i = 0; i < 5; i++) {
        sqlite3_stmt *ins = NULL;
        KS_ASSERT_EQ_INT(sqlite3_prepare_v2(legacy, "INSERT INTO key_events (key_code, app_name, timestamp) VALUES (65, 'LegacyApp', ?)", -1, &ins, NULL), SQLITE_OK);
        sqlite3_bind_int64(ins, 1, ts_base + i);
        KS_ASSERT_EQ_INT(sqlite3_step(ins), SQLITE_DONE);
        sqlite3_finalize(ins);
    }
    sqlite3_close(legacy);

    // Init KsDB -> backfills rollups & drops idx_key_events_key_code
    KsDB *db = ks_db_init(dir);
    KS_ASSERT_TRUE(db != NULL);
    KS_ASSERT_EQ_INT(ks_db_version(db), 1);

    // Push 260 events across >2 chunks
    for (int i = 0; i < 260; i++) {
        ks_db_push_event(db, 66 + (i % 3), "Editor", ts_base + (int64_t)i * 1000LL);
    }
    ks_db_close(db);

    KsDB *db2 = ks_db_init(dir);
    KS_ASSERT_TRUE(db2 != NULL);

    KsTodaySummary sum;
    KS_ASSERT_TRUE(ks_stats_query_all_conn(db2->conn, &sum));
    KS_ASSERT_EQ_INT(sum.total_keys, 265);
    KS_ASSERT_TRUE(sum.app_breakdown_len >= 2);
    KS_ASSERT_EQ_STR(sum.app_breakdown[0].app_name, "Editor");
    KS_ASSERT_EQ_INT(sum.app_breakdown[0].count, 260);

    // Check dropped index is gone
    sqlite3_stmt *idx_st = NULL;
    KS_ASSERT_EQ_INT(sqlite3_prepare_v2(db2->conn, "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name='idx_key_events_key_code'", -1, &idx_st, NULL), SQLITE_OK);
    KS_ASSERT_EQ_INT(sqlite3_step(idx_st), SQLITE_ROW);
    KS_ASSERT_EQ_INT(sqlite3_column_int(idx_st, 0), 0);
    sqlite3_finalize(idx_st);

    uint64_t v_before = ks_db_version(db2);
    KS_ASSERT_TRUE(ks_db_reset(db2));
    KS_ASSERT_TRUE(ks_db_version(db2) > v_before);

    KS_ASSERT_TRUE(ks_stats_query_all_conn(db2->conn, &sum));
    KS_ASSERT_EQ_INT(sum.total_keys, 0);

    ks_db_close(db2);
    return true;
}

static bool test_stats_continuous_and_vks(void) {
    char buf[KS_KEY_NAME_CAP];
    KS_ASSERT_EQ_STR(ks_vk_to_name('A', buf, sizeof(buf)), "A");
    KS_ASSERT_EQ_STR(ks_vk_to_name(96, buf, sizeof(buf)), "0");
    KS_ASSERT_EQ_STR(ks_vk_to_name(109, buf, sizeof(buf)), "Num-");
    KS_ASSERT_EQ_STR(ks_vk_to_name(189, buf, sizeof(buf)), "-");
    KS_ASSERT_EQ_STR(ks_vk_to_name(112, buf, sizeof(buf)), "F1");
    KS_ASSERT_EQ_STR(ks_vk_to_name(160, buf, sizeof(buf)), "LShift");
    KS_ASSERT_EQ_STR(ks_vk_to_name(255, buf, sizeof(buf)), "Fn");

    // Continuous recording accumulates across arbitrary days without calendar cutoff
    SYSTEMTIME st_day2 = { .wYear = 2026, .wMonth = 11, .wDay = 14, .wHour = 18, .wMinute = 40, .wSecond = 0 };
    FILETIME ft;
    SystemTimeToFileTime(&st_day2, &ft);
    ULARGE_INTEGER ui = { .LowPart = ft.dwLowDateTime, .HighPart = ft.dwHighDateTime };
    int64_t ts_day2 = (int64_t)((ui.QuadPart - 116444736000000000ULL) / 10000ULL);
    int64_t ts_day1 = ts_day2 - 3LL * 86400000LL;

    wchar_t dir[MAX_PATH];
    make_temp_dir_w(L"stats", dir, MAX_PATH);
    KsDB *db = ks_db_init(dir);
    KS_ASSERT_TRUE(db != NULL);
    ks_db_push_event(db, 'A', "Code", ts_day2);
    ks_db_push_event(db, 'A', "Code", ts_day2 + 60000LL);
    ks_db_push_event(db, 'B', "Browser", ts_day1);
    ks_db_close(db);

    KsDB *db2 = ks_db_init(dir);
    KS_ASSERT_TRUE(db2 != NULL);

    KsStatsCache cache;
    memset(&cache, 0, sizeof(cache));
    ks_stats_cache_invalidate(&cache);
    KsTodaySummary sum;
    KS_ASSERT_TRUE(ks_stats_get_all_cached(db2, &cache, &sum));
    KS_ASSERT_EQ_INT(sum.total_keys, 3);
    KS_ASSERT_EQ_STR(sum.app_breakdown[0].app_name, "Code");
    KS_ASSERT_EQ_INT(sum.app_breakdown[0].count, 2);
    KS_ASSERT_TRUE(ks_stats_get_all_cached(db2, &cache, &sum));
    KS_ASSERT_EQ_INT(sum.total_keys, 3);

    // Also test raw key_events fallback without rollup tables
    sqlite3 *mem = NULL;
    KS_ASSERT_EQ_INT(sqlite3_open(":memory:", &mem), SQLITE_OK);
    KS_ASSERT_EQ_INT(sqlite3_exec(mem, "CREATE TABLE key_events (id INTEGER PRIMARY KEY, key_code INTEGER, app_name TEXT, timestamp INTEGER);", NULL, NULL, NULL), SQLITE_OK);
    KS_ASSERT_EQ_INT(sqlite3_exec(mem, "INSERT INTO key_events (key_code, app_name, timestamp) VALUES (65, 'App', 1000);", NULL, NULL, NULL), SQLITE_OK);
    KS_ASSERT_TRUE(ks_stats_query_all_conn(mem, &sum));
    KS_ASSERT_EQ_INT(sum.total_keys, 1);
    sqlite3_close(mem);

    ks_db_close(db2);
    return true;
}

static bool phys_none(int vk) { (void)vk; return false; }
static bool phys_lshift(int vk) { return vk == 160; }

static bool test_keyboard_flash_and_bitmask(void) {
    KsKeyboardService *svc = ks_keyboard_create(NULL);
    KS_ASSERT_TRUE(svc != NULL);
    ks_keyboard_set_paused(svc, true);

    char names[16][KS_KEY_NAME_CAP];
    bool any = false;

    ks_keyboard_record_visual_down(svc, 'A', 1000);
    ks_keyboard_record_visual_up(svc, 'A');

    int n = ks_keyboard_active_keys_at(svc, 1100, phys_none, names, 16, &any);
    KS_ASSERT_EQ_INT(n, 1);
    KS_ASSERT_EQ_STR(names[0], "A");
    KS_ASSERT_TRUE(any);

    n = ks_keyboard_active_keys_at(svc, 1180, phys_none, names, 16, &any);
    KS_ASSERT_EQ_INT(n, 0);
    KS_ASSERT_TRUE(!any);

    // Held key beyond flash window
    ks_keyboard_record_visual_down(svc, 160, 2000);
    n = ks_keyboard_active_keys_at(svc, 2500, phys_lshift, names, 16, &any);
    KS_ASSERT_EQ_INT(n, 1);
    KS_ASSERT_EQ_STR(names[0], "LShift");

    // Stuck key recovery when OS says released
    n = ks_keyboard_active_keys_at(svc, 2550, phys_none, names, 16, &any);
    KS_ASSERT_EQ_INT(n, 0);
    KS_ASSERT_EQ_INT(atomic_load(&svc->active_mask[0]), 0);
    KS_ASSERT_EQ_INT(atomic_load(&svc->active_mask[2]), 0);

    // Boundary VK 254 (word 3)
    ks_keyboard_record_visual_down(svc, 254, 3000);
    ks_keyboard_record_visual_up(svc, 254);
    n = ks_keyboard_active_keys_at(svc, 3050, phys_none, names, 16, &any);
    KS_ASSERT_EQ_INT(n, 1);
    n = ks_keyboard_active_keys_at(svc, 3300, phys_none, names, 16, &any);
    KS_ASSERT_EQ_INT(n, 0);
    KS_ASSERT_EQ_INT(atomic_load(&svc->active_mask[3]), 0);

    ks_keyboard_destroy(svc);
    return true;
}

int main(void) {
    KS_RUN_TEST(test_config_resolve_and_roundtrip);
    KS_RUN_TEST(test_db_rollup_backfill_flush_reset);
    KS_RUN_TEST(test_stats_continuous_and_vks);
    KS_RUN_TEST(test_keyboard_flash_and_bitmask);

    printf("\nCore tests: %d run, %d failed\n", g_ks_tests_run, g_ks_tests_failed);
    return g_ks_tests_failed == 0 ? 0 : 1;
}

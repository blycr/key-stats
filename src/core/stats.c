#include "stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *ks_vk_to_name(int vk, char *buf, size_t cap) {
    if (vk >= 'A' && vk <= 'Z') {
        snprintf(buf, cap, "%c", (char)vk);
        return buf;
    }
    if (vk >= '0' && vk <= '9') {
        snprintf(buf, cap, "%c", (char)vk);
        return buf;
    }
    if (vk >= 96 && vk <= 105) {
        snprintf(buf, cap, "%c", (char)('0' + (vk - 96)));
        return buf;
    }
    if (vk >= 112 && vk <= 135) {
        snprintf(buf, cap, "F%d", vk - 111);
        return buf;
    }
    switch (vk) {
    case 32:  return "Space";
    case 13:  return "Enter";
    case 8:   return "Back";
    case 9:   return "Tab";
    case 27:  return "Esc";
    case 20:  return "Caps";
    case 91:  return "LWin";
    case 92:  return "RWin";
    case 93:  return "Menu";
    case 95:  return "Sleep";
    case 160: return "LShift";
    case 161: return "RShift";
    case 162: return "LCtrl";
    case 163: return "RCtrl";
    case 164: return "LAlt";
    case 165: return "RAlt";
    case 16:  return "Shift";
    case 17:  return "Ctrl";
    case 18:  return "Alt";
    case 33:  return "PgUp";
    case 34:  return "PgDn";
    case 35:  return "End";
    case 36:  return "Home";
    case 37:  return "Left";
    case 38:  return "Up";
    case 39:  return "Right";
    case 40:  return "Down";
    case 45:  return "Insert";
    case 46:  return "Delete";
    case 44:  return "PrtSc";
    case 19:  return "Pause";
    case 145: return "ScrLk";
    case 144: return "NumLk";
    case 106: return "Num*";
    case 107: return "Num+";
    case 109: return "Num-";
    case 110: return "Num.";
    case 111: return "Num/";
    case 12:  return "Clear";
    case 187: return "=";
    case 189: return "-";
    case 190: return ".";
    case 188: return ",";
    case 191: return "/";
    case 186: return ";";
    case 192: return "`";
    case 219: return "[";
    case 220: return "\\";
    case 221: return "]";
    case 222: return "'";
    case 223: return "~";
    case 166: return "BrwBack";
    case 167: return "BrwFwd";
    case 168: return "BrwRef";
    case 169: return "BrwStop";
    case 170: return "BrwSrch";
    case 171: return "BrwFav";
    case 172: return "BrwHome";
    case 173: return "Mute";
    case 174: return "VolDn";
    case 175: return "VolUp";
    case 176: return "Next";
    case 177: return "Prev";
    case 178: return "Stop";
    case 179: return "Play";
    case 180: return "Mail";
    case 181: return "Media";
    case 182: return "Launch1";
    case 183: return "Launch2";
    case 255: return "Fn";
    default:
        snprintf(buf, cap, "Key-%d", vk);
        return buf;
    }
}

static int cmp_key_count_desc(const void *a, const void *b) {
    const KsKeyCount *ka = (const KsKeyCount *)a;
    const KsKeyCount *kb = (const KsKeyCount *)b;
    if (ka->count != kb->count) {
        return (kb->count > ka->count) ? 1 : -1;
    }
    return strcmp(ka->key_name, kb->key_name);
}

static bool has_rollup_table(sqlite3 *conn) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name='key_stats_buckets' LIMIT 1",
            -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

bool ks_stats_query_all_conn(sqlite3 *conn, KsTodaySummary *out) {
    if (!conn || !out) return false;
    memset(out, 0, sizeof(*out));

    bool use_rollups = has_rollup_table(conn);

    const char *key_sql = use_rollups
        ? "SELECT key_code, SUM(count) as count FROM key_stats_buckets GROUP BY key_code"
        : "SELECT key_code, COUNT(*) as count FROM key_events GROUP BY key_code";

    sqlite3_stmt *kstmt = NULL;
    if (sqlite3_prepare_v2(conn, key_sql, -1, &kstmt, NULL) != SQLITE_OK) return false;

    char tmp_name[KS_KEY_NAME_CAP];
    while (sqlite3_step(kstmt) == SQLITE_ROW) {
        int kc = sqlite3_column_int(kstmt, 0);
        int cnt = sqlite3_column_int(kstmt, 1);
        out->total_keys += cnt;

        const char *name = ks_vk_to_name(kc, tmp_name, sizeof(tmp_name));
        bool merged = false;
        for (int i = 0; i < out->all_keys_len; i++) {
            if (strcmp(out->all_keys[i].key_name, name) == 0) {
                out->all_keys[i].count += cnt;
                merged = true;
                break;
            }
        }
        if (!merged && out->all_keys_len < KS_MAX_ALL_KEYS) {
            snprintf(out->all_keys[out->all_keys_len].key_name, KS_KEY_NAME_CAP, "%s", name);
            out->all_keys[out->all_keys_len].count = cnt;
            out->all_keys_len++;
        }
    }
    sqlite3_finalize(kstmt);

    if (out->all_keys_len > 1) {
        qsort(out->all_keys, (size_t)out->all_keys_len, sizeof(KsKeyCount), cmp_key_count_desc);
    }
    out->top_keys_len = out->all_keys_len < KS_MAX_TOP_KEYS ? out->all_keys_len : KS_MAX_TOP_KEYS;
    for (int i = 0; i < out->top_keys_len; i++) {
        out->top_keys[i] = out->all_keys[i];
    }

    const char *app_sql = use_rollups
        ? "SELECT app_name, SUM(count) as count FROM app_stats_buckets GROUP BY app_name ORDER BY count DESC, app_name ASC LIMIT 10"
        : "SELECT app_name, COUNT(*) as count FROM key_events GROUP BY app_name ORDER BY count DESC, app_name ASC LIMIT 10";

    sqlite3_stmt *astmt = NULL;
    if (sqlite3_prepare_v2(conn, app_sql, -1, &astmt, NULL) != SQLITE_OK) return false;

    while (sqlite3_step(astmt) == SQLITE_ROW && out->app_breakdown_len < KS_MAX_TOP_APPS) {
        const unsigned char *app = sqlite3_column_text(astmt, 0);
        int cnt = sqlite3_column_int(astmt, 1);
        snprintf(out->app_breakdown[out->app_breakdown_len].app_name, KS_APP_NAME_CAP,
                 "%s", app ? (const char *)app : "Unknown");
        out->app_breakdown[out->app_breakdown_len].count = cnt;
        out->app_breakdown_len++;
    }
    sqlite3_finalize(astmt);
    return true;
}

static bool query_all_conn_cb(sqlite3 *conn, void *ctx) {
    return ks_stats_query_all_conn(conn, (KsTodaySummary *)ctx);
}

bool ks_stats_get_all_cached(KsDB *db, KsStatsCache *cache, KsTodaySummary *out) {
    if (!db || !out) return false;

    uint64_t ver = ks_db_version(db);
    if (cache && cache->valid && cache->db_version == ver) {
        *out = cache->summary;
        return true;
    }

    if (!ks_db_with_conn(db, query_all_conn_cb, out)) return false;

    if (cache) {
        cache->valid = true;
        cache->db_version = ver;
        cache->summary = *out;
    }
    return true;
}

void ks_stats_cache_invalidate(KsStatsCache *cache) {
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
}

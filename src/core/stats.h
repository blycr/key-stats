#ifndef KS_STATS_H
#define KS_STATS_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KS_MAX_TOP_KEYS 10
#define KS_MAX_TOP_APPS 10
#define KS_MAX_ALL_KEYS 256
#define KS_KEY_NAME_CAP 24

typedef struct {
    char key_name[KS_KEY_NAME_CAP];
    int count;
} KsKeyCount;

typedef struct {
    char app_name[KS_APP_NAME_CAP];
    int count;
} KsAppCount;

typedef struct {
    int total_keys;
    KsKeyCount top_keys[KS_MAX_TOP_KEYS];
    int top_keys_len;
    KsKeyCount all_keys[KS_MAX_ALL_KEYS];
    int all_keys_len;
    KsAppCount app_breakdown[KS_MAX_TOP_APPS];
    int app_breakdown_len;
} KsTodaySummary;

typedef struct {
    bool valid;
    uint64_t db_version;
    KsTodaySummary summary;
} KsStatsCache;

// Maps Windows Virtual Key code to canonical KeyStats display name.
const char *ks_vk_to_name(int vk, char *buf, size_t cap);

// Queries continuous all-time statistics from SQLite (using 15-min rollups when present, else raw events).
bool ks_stats_query_all_conn(sqlite3 *conn, KsTodaySummary *out);

// Cached continuous query against KsDB keyed by monotonic db_version.
bool ks_stats_get_all_cached(KsDB *db, KsStatsCache *cache, KsTodaySummary *out);
void ks_stats_cache_invalidate(KsStatsCache *cache);

#ifdef __cplusplus
}
#endif

#endif // KS_STATS_H

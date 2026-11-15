#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KS_INSERT_CHUNK_SIZE 100
#define KS_FLUSH_BATCH_SIZE 256
#define KS_FLUSH_INTERVAL_MS 500
#define KS_ROLLUP_VERSION "1"

int64_t ks_bucket_timestamp(int64_t ts_ms) {
    int64_t m = ts_ms % KS_BUCKET_SIZE_MS;
    if (m < 0) m += KS_BUCKET_SIZE_MS;
    return ts_ms - m;
}

static void copy_utf8_safe(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
        // Avoid splitting a multi-byte UTF-8 sequence
        while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) {
            n--;
        }
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static bool exec_sql(sqlite3 *conn, const char *sql) {
    char *errmsg = NULL;
    int rc = sqlite3_exec(conn, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg) sqlite3_free(errmsg);
        return false;
    }
    return true;
}

static bool ensure_rollup_backfill(sqlite3 *conn) {
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(conn, "SELECT value FROM meta WHERE key = 'rollup_version'", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;

    bool up_to_date = false;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char *val = sqlite3_column_text(stmt, 0);
        if (val && strcmp((const char *)val, KS_ROLLUP_VERSION) == 0) {
            up_to_date = true;
        }
    }
    sqlite3_finalize(stmt);
    if (up_to_date) return true;

    if (!exec_sql(conn, "BEGIN IMMEDIATE")) return false;

    bool ok = exec_sql(conn, "DELETE FROM key_stats_buckets") &&
              exec_sql(conn, "DELETE FROM app_stats_buckets");

    if (ok) {
        sqlite3_stmt *ins_key = NULL;
        if (sqlite3_prepare_v2(conn,
                "INSERT INTO key_stats_buckets (bucket_ts, key_code, count) "
                "SELECT (timestamp / ?) * ?, key_code, COUNT(*) FROM key_events GROUP BY 1, key_code",
                -1, &ins_key, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(ins_key, 1, KS_BUCKET_SIZE_MS);
            sqlite3_bind_int64(ins_key, 2, KS_BUCKET_SIZE_MS);
            ok = (sqlite3_step(ins_key) == SQLITE_DONE);
            sqlite3_finalize(ins_key);
        } else {
            ok = false;
        }
    }

    if (ok) {
        sqlite3_stmt *ins_app = NULL;
        if (sqlite3_prepare_v2(conn,
                "INSERT INTO app_stats_buckets (bucket_ts, app_name, count) "
                "SELECT (timestamp / ?) * ?, app_name, COUNT(*) FROM key_events GROUP BY 1, app_name",
                -1, &ins_app, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(ins_app, 1, KS_BUCKET_SIZE_MS);
            sqlite3_bind_int64(ins_app, 2, KS_BUCKET_SIZE_MS);
            ok = (sqlite3_step(ins_app) == SQLITE_DONE);
            sqlite3_finalize(ins_app);
        } else {
            ok = false;
        }
    }

    if (ok) {
        ok = exec_sql(conn,
            "INSERT INTO meta (key, value) VALUES ('rollup_version', '" KS_ROLLUP_VERSION "') "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    }

    if (ok) {
        ok = exec_sql(conn, "COMMIT");
    } else {
        exec_sql(conn, "ROLLBACK");
    }
    return ok;
}

typedef struct {
    int64_t bucket_ts;
    int key_code;
    int count;
} KeyAggEntry;

typedef struct {
    int64_t bucket_ts;
    char app_name[KS_APP_NAME_CAP];
    int count;
} AppAggEntry;

static bool build_multi_row_sql(char *buf, size_t cap, const char *prefix, size_t rows, const char *suffix) {
    size_t pos = 0;
    int n = snprintf(buf, cap, "%s", prefix);
    if (n < 0 || (size_t)n >= cap) return false;
    pos = (size_t)n;
    for (size_t i = 0; i < rows; i++) {
        const char *part = (i == 0) ? "(?,?,?)" : ",(?,?,?)";
        size_t plen = strlen(part);
        if (pos + plen >= cap) return false;
        memcpy(buf + pos, part, plen);
        pos += plen;
    }
    size_t slen = strlen(suffix);
    if (pos + slen >= cap) return false;
    memcpy(buf + pos, suffix, slen + 1);
    return true;
}

static bool insert_key_events_chunked(sqlite3 *conn, const KsKeyEvent *batch, size_t count) {
    char sql[1024];
    for (size_t start = 0; start < count; start += KS_INSERT_CHUNK_SIZE) {
        size_t n = count - start;
        if (n > KS_INSERT_CHUNK_SIZE) n = KS_INSERT_CHUNK_SIZE;
        if (!build_multi_row_sql(sql, sizeof(sql),
                "INSERT INTO key_events (key_code, app_name, timestamp) VALUES ", n, "")) {
            return false;
        }
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
        for (size_t i = 0; i < n; i++) {
            int base = (int)(i * 3);
            sqlite3_bind_int(stmt, base + 1, batch[start + i].key_code);
            sqlite3_bind_text(stmt, base + 2, batch[start + i].app_name, -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, base + 3, batch[start + i].timestamp);
        }
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) return false;
    }
    return true;
}

static bool upsert_rollup_buckets(sqlite3 *conn, const KsKeyEvent *batch, size_t count) {
    KeyAggEntry *key_agg = (KeyAggEntry *)calloc(count, sizeof(KeyAggEntry));
    AppAggEntry *app_agg = (AppAggEntry *)calloc(count, sizeof(AppAggEntry));
    if (!key_agg || !app_agg) {
        free(key_agg);
        free(app_agg);
        return false;
    }

    size_t key_len = 0;
    size_t app_len = 0;
    for (size_t i = 0; i < count; i++) {
        int64_t bts = ks_bucket_timestamp(batch[i].timestamp);
        int kc = batch[i].key_code;
        bool kfound = false;
        for (size_t j = 0; j < key_len; j++) {
            if (key_agg[j].bucket_ts == bts && key_agg[j].key_code == kc) {
                key_agg[j].count++;
                kfound = true;
                break;
            }
        }
        if (!kfound) {
            key_agg[key_len++] = (KeyAggEntry){ .bucket_ts = bts, .key_code = kc, .count = 1 };
        }

        bool afound = false;
        for (size_t j = 0; j < app_len; j++) {
            if (app_agg[j].bucket_ts == bts && strcmp(app_agg[j].app_name, batch[i].app_name) == 0) {
                app_agg[j].count++;
                afound = true;
                break;
            }
        }
        if (!afound) {
            app_agg[app_len].bucket_ts = bts;
            snprintf(app_agg[app_len].app_name, KS_APP_NAME_CAP, "%s", batch[i].app_name);
            app_agg[app_len].count = 1;
            app_len++;
        }
    }

    bool ok = true;
    char sql[1280];

    for (size_t start = 0; ok && start < key_len; start += KS_INSERT_CHUNK_SIZE) {
        size_t n = key_len - start;
        if (n > KS_INSERT_CHUNK_SIZE) n = KS_INSERT_CHUNK_SIZE;
        ok = build_multi_row_sql(sql, sizeof(sql),
                "INSERT INTO key_stats_buckets (bucket_ts, key_code, count) VALUES ", n,
                " ON CONFLICT(bucket_ts, key_code) DO UPDATE SET count = key_stats_buckets.count + excluded.count");
        if (!ok) break;
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL) != SQLITE_OK) { ok = false; break; }
        for (size_t i = 0; i < n; i++) {
            int base = (int)(i * 3);
            sqlite3_bind_int64(stmt, base + 1, key_agg[start + i].bucket_ts);
            sqlite3_bind_int(stmt, base + 2, key_agg[start + i].key_code);
            sqlite3_bind_int(stmt, base + 3, key_agg[start + i].count);
        }
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }

    for (size_t start = 0; ok && start < app_len; start += KS_INSERT_CHUNK_SIZE) {
        size_t n = app_len - start;
        if (n > KS_INSERT_CHUNK_SIZE) n = KS_INSERT_CHUNK_SIZE;
        ok = build_multi_row_sql(sql, sizeof(sql),
                "INSERT INTO app_stats_buckets (bucket_ts, app_name, count) VALUES ", n,
                " ON CONFLICT(bucket_ts, app_name) DO UPDATE SET count = app_stats_buckets.count + excluded.count");
        if (!ok) break;
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL) != SQLITE_OK) { ok = false; break; }
        for (size_t i = 0; i < n; i++) {
            int base = (int)(i * 3);
            sqlite3_bind_int64(stmt, base + 1, app_agg[start + i].bucket_ts);
            sqlite3_bind_text(stmt, base + 2, app_agg[start + i].app_name, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, base + 3, app_agg[start + i].count);
        }
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }

    free(key_agg);
    free(app_agg);
    return ok;
}

static void flush_batch(KsDB *db, const KsKeyEvent *batch, size_t count) {
    if (!db || count == 0) return;

    EnterCriticalSection(&db->db_lock);
    bool committed = false;
    if (exec_sql(db->conn, "BEGIN IMMEDIATE")) {
        if (insert_key_events_chunked(db->conn, batch, count) &&
            upsert_rollup_buckets(db->conn, batch, count) &&
            exec_sql(db->conn, "COMMIT")) {
            committed = true;
        } else {
            exec_sql(db->conn, "ROLLBACK");
        }
    }
    if (committed) {
        atomic_fetch_add(&db->version, 1);
    }
    KsDBCommitCallback cb = db->on_commit;
    void *ud = db->on_commit_userdata;
    LeaveCriticalSection(&db->db_lock);

    if (committed && cb) {
        cb(ud);
    }
}

static size_t pop_batch(KsDB *db, KsKeyEvent *out, size_t max_items) {
    size_t n = 0;
    while (n < max_items && db->q_count > 0) {
        out[n++] = db->queue[db->q_head];
        db->q_head = (db->q_head + 1) % KS_EVENT_QUEUE_CAP;
        db->q_count--;
    }
    return n;
}

static DWORD WINAPI db_writer_proc(LPVOID param) {
    KsDB *db = (KsDB *)param;
    KsKeyEvent batch[KS_FLUSH_BATCH_SIZE];

    for (;;) {
        size_t n = 0;
        EnterCriticalSection(&db->queue_lock);
        while (db->q_count == 0 && !atomic_load(&db->closing)) {
            SleepConditionVariableCS(&db->queue_cv, &db->queue_lock, KS_FLUSH_INTERVAL_MS);
            if (db->q_count > 0 || atomic_load(&db->closing)) break;
        }
        if (db->q_count > 0) {
            n = pop_batch(db, batch, KS_FLUSH_BATCH_SIZE);
        }
        bool done = atomic_load(&db->closing) && db->q_count == 0;
        LeaveCriticalSection(&db->queue_lock);

        if (n > 0) {
            flush_batch(db, batch, n);
        }
        if (done) break;
    }
    return 0;
}

KsDB *ks_db_init(const wchar_t *data_dir) {
    if (!data_dir || !ks_mkdir_recursive_w(data_dir)) return NULL;

    wchar_t db_path_w[MAX_PATH];
    _snwprintf(db_path_w, MAX_PATH, L"%ls\\data.db", data_dir);
    db_path_w[MAX_PATH - 1] = L'\0';

    char *db_path_utf8 = ks_wide_to_utf8_alloc(db_path_w);
    if (!db_path_utf8) return NULL;

    sqlite3 *conn = NULL;
    int rc = sqlite3_open_v2(db_path_utf8, &conn,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             NULL);
    free(db_path_utf8);
    if (rc != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        return NULL;
    }

    exec_sql(conn, "PRAGMA journal_mode=WAL;");
    exec_sql(conn, "PRAGMA synchronous=NORMAL;");
    exec_sql(conn, "PRAGMA busy_timeout=5000;");
    exec_sql(conn, "PRAGMA cache_size=-512;");
    exec_sql(conn, "PRAGMA mmap_size=0;");
    exec_sql(conn, "PRAGMA temp_store=MEMORY;");

    const char *schema_sql =
        "CREATE TABLE IF NOT EXISTS key_events ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  key_code INTEGER NOT NULL,"
        "  app_name TEXT NOT NULL,"
        "  timestamp INTEGER NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_key_events_timestamp ON key_events (timestamp);"
        "DROP INDEX IF EXISTS idx_key_events_key_code;"
        "CREATE TABLE IF NOT EXISTS meta ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS key_stats_buckets ("
        "  bucket_ts INTEGER NOT NULL,"
        "  key_code INTEGER NOT NULL,"
        "  count INTEGER NOT NULL,"
        "  PRIMARY KEY (bucket_ts, key_code)"
        ") WITHOUT ROWID;"
        "CREATE TABLE IF NOT EXISTS app_stats_buckets ("
        "  bucket_ts INTEGER NOT NULL,"
        "  app_name TEXT NOT NULL,"
        "  count INTEGER NOT NULL,"
        "  PRIMARY KEY (bucket_ts, app_name)"
        ") WITHOUT ROWID;";

    if (!exec_sql(conn, schema_sql) || !ensure_rollup_backfill(conn)) {
        sqlite3_close(conn);
        return NULL;
    }

    KsDB *db = (KsDB *)calloc(1, sizeof(KsDB));
    if (!db) {
        sqlite3_close(conn);
        return NULL;
    }
    db->conn = conn;
    InitializeCriticalSection(&db->db_lock);
    InitializeCriticalSection(&db->queue_lock);
    InitializeConditionVariable(&db->queue_cv);
    atomic_init(&db->closing, false);
    atomic_init(&db->closed, false);
    atomic_init(&db->version, 1);
    atomic_init(&db->drop_count, 0);

    db->writer_thread = CreateThread(NULL, 0, db_writer_proc, db, 0, NULL);
    if (!db->writer_thread) {
        DeleteCriticalSection(&db->queue_lock);
        DeleteCriticalSection(&db->db_lock);
        sqlite3_close(conn);
        free(db);
        return NULL;
    }
    return db;
}

void ks_db_set_commit_callback(KsDB *db, KsDBCommitCallback cb, void *userdata) {
    if (!db) return;
    EnterCriticalSection(&db->db_lock);
    db->on_commit = cb;
    db->on_commit_userdata = userdata;
    LeaveCriticalSection(&db->db_lock);
}

void ks_db_push_event(KsDB *db, int key_code, const char *app_name, int64_t timestamp) {
    if (!db || atomic_load(&db->closed)) return;

    EnterCriticalSection(&db->queue_lock);
    if (atomic_load(&db->closed) || db->q_count >= KS_EVENT_QUEUE_CAP) {
        if (db->q_count >= KS_EVENT_QUEUE_CAP) {
            atomic_fetch_add(&db->drop_count, 1);
        }
        LeaveCriticalSection(&db->queue_lock);
        return;
    }
    KsKeyEvent *slot = &db->queue[db->q_tail];
    slot->key_code = key_code;
    slot->timestamp = timestamp;
    copy_utf8_safe(slot->app_name, sizeof(slot->app_name), app_name);
    db->q_tail = (db->q_tail + 1) % KS_EVENT_QUEUE_CAP;
    db->q_count++;
    bool should_wake = (db->q_count >= KS_FLUSH_BATCH_SIZE);
    LeaveCriticalSection(&db->queue_lock);

    if (should_wake) {
        WakeConditionVariable(&db->queue_cv);
    }
}

uint64_t ks_db_version(const KsDB *db) {
    if (!db) return 0;
    return atomic_load(&db->version);
}

bool ks_db_reset(KsDB *db) {
    if (!db || atomic_load(&db->closed)) return false;
    EnterCriticalSection(&db->db_lock);
    bool ok = false;
    if (exec_sql(db->conn, "BEGIN IMMEDIATE")) {
        if (exec_sql(db->conn, "DELETE FROM key_events") &&
            exec_sql(db->conn, "DELETE FROM key_stats_buckets") &&
            exec_sql(db->conn, "DELETE FROM app_stats_buckets") &&
            exec_sql(db->conn, "COMMIT")) {
            ok = true;
        } else {
            exec_sql(db->conn, "ROLLBACK");
        }
    }
    if (ok) {
        atomic_fetch_add(&db->version, 1);
    }
    KsDBCommitCallback cb = db->on_commit;
    void *ud = db->on_commit_userdata;
    LeaveCriticalSection(&db->db_lock);

    if (ok && cb) cb(ud);
    return ok;
}

void ks_db_close(KsDB *db) {
    if (!db) return;
    bool expected = false;
    if (!atomic_compare_exchange_strong(&db->closed, &expected, true)) {
        return;
    }
    atomic_store(&db->closing, true);
    EnterCriticalSection(&db->queue_lock);
    WakeAllConditionVariable(&db->queue_cv);
    LeaveCriticalSection(&db->queue_lock);

    if (db->writer_thread) {
        WaitForSingleObject(db->writer_thread, 5000);
        CloseHandle(db->writer_thread);
        db->writer_thread = NULL;
    }

    // Drain any residual events safely
    KsKeyEvent batch[KS_FLUSH_BATCH_SIZE];
    for (;;) {
        EnterCriticalSection(&db->queue_lock);
        size_t n = pop_batch(db, batch, KS_FLUSH_BATCH_SIZE);
        LeaveCriticalSection(&db->queue_lock);
        if (n == 0) break;
        flush_batch(db, batch, n);
    }

    EnterCriticalSection(&db->db_lock);
    if (db->conn) {
        sqlite3_close(db->conn);
        db->conn = NULL;
    }
    LeaveCriticalSection(&db->db_lock);

    DeleteCriticalSection(&db->queue_lock);
    DeleteCriticalSection(&db->db_lock);
    free(db);
}

bool ks_db_with_conn(KsDB *db, bool (*fn)(sqlite3 *conn, void *ctx), void *ctx) {
    if (!db || !fn || atomic_load(&db->closed)) return false;
    EnterCriticalSection(&db->db_lock);
    bool ok = false;
    if (db->conn) ok = fn(db->conn, ctx);
    LeaveCriticalSection(&db->db_lock);
    return ok;
}

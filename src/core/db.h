#ifndef KS_DB_H
#define KS_DB_H

#include "win32_util.h"
#include "../../third_party/sqlite/sqlite3.h"
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KS_BUCKET_SIZE_MS ((int64_t)(15 * 60 * 1000))
#define KS_EVENT_QUEUE_CAP 4096
#define KS_APP_NAME_CAP 128

typedef struct {
    int key_code;
    int64_t timestamp;
    char app_name[KS_APP_NAME_CAP];
} KsKeyEvent;

typedef void (*KsDBCommitCallback)(void *userdata);

typedef struct {
    sqlite3 *conn;
    CRITICAL_SECTION db_lock;

    // Ring buffer between keyboard hook thread and DB writer thread
    CRITICAL_SECTION queue_lock;
    CONDITION_VARIABLE queue_cv;
    KsKeyEvent queue[KS_EVENT_QUEUE_CAP];
    size_t q_head;
    size_t q_tail;
    size_t q_count;

    HANDLE writer_thread;
    atomic_bool closing;
    atomic_bool closed;
    atomic_uint_fast64_t version;
    atomic_int_fast64_t drop_count;

    KsDBCommitCallback on_commit;
    void *on_commit_userdata;
} KsDB;

int64_t ks_bucket_timestamp(int64_t ts_ms);

KsDB *ks_db_init(const wchar_t *data_dir);
void ks_db_set_commit_callback(KsDB *db, KsDBCommitCallback cb, void *userdata);

// Safe to call from the WH_KEYBOARD_LL hook thread: never blocks on SQLite or disk I/O.
void ks_db_push_event(KsDB *db, int key_code, const char *app_name, int64_t timestamp);

uint64_t ks_db_version(const KsDB *db);
bool ks_db_reset(KsDB *db);
void ks_db_close(KsDB *db);

// Executes a read callback while holding db_lock.
bool ks_db_with_conn(KsDB *db, bool (*fn)(sqlite3 *conn, void *ctx), void *ctx);

#ifdef __cplusplus
}
#endif

#endif // KS_DB_H

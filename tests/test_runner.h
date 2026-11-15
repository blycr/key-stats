#ifndef KS_TEST_RUNNER_H
#define KS_TEST_RUNNER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int g_ks_tests_run = 0;
static int g_ks_tests_failed = 0;

#define KS_ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); \
        return false; \
    } \
} while (0)

#define KS_ASSERT_EQ_INT(got, want) do { \
    long long _g = (long long)(got); \
    long long _w = (long long)(want); \
    if (_g != _w) { \
        fprintf(stderr, "  FAIL %s:%d: %s = %lld, want %lld\n", __FILE__, __LINE__, #got, _g, _w); \
        return false; \
    } \
} while (0)

#define KS_ASSERT_EQ_STR(got, want) do { \
    const char *_g = (got); \
    const char *_w = (want); \
    if (!_g || !_w || strcmp(_g, _w) != 0) { \
        fprintf(stderr, "  FAIL %s:%d: %s = \"%s\", want \"%s\"\n", __FILE__, __LINE__, #got, _g ? _g : "(null)", _w ? _w : "(null)"); \
        return false; \
    } \
} while (0)

#define KS_RUN_TEST(fn) do { \
    g_ks_tests_run++; \
    printf("=== RUN   %s\n", #fn); \
    if (fn()) { \
        printf("--- PASS: %s\n", #fn); \
    } else { \
        printf("--- FAIL: %s\n", #fn); \
        g_ks_tests_failed++; \
    } \
} while (0)

#endif // KS_TEST_RUNNER_H

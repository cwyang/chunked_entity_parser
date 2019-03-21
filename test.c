#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "picotest/picotest.h"
#include "chunked_parser.h"

static void test_chunked_at_once(int line, int consume_trailer, const char *encoded,
                                 ssize_t expected_ret, ssize_t expected_rem)
{
    struct phr_chunked_decoder dec = {0};
    char *buf;
    size_t bufsz, parse_sz = 0;
    ssize_t ret;

    dec.consume_trailer = consume_trailer;

    note("testing at-once, source at line %d", line);

    buf = strdup(encoded);
    bufsz = strlen(buf);

    ret = ho_parse_chunked(&dec, buf, bufsz, &parse_sz);

    ok(ret == expected_ret);
    ok(bufsz - parse_sz == expected_rem);

    free(buf);
}

static void test_chunked_per_byte(int line, int consume_trailer, const char *encoded,
                                  ssize_t expected_ret, ssize_t expected_rem)
{
    struct phr_chunked_decoder dec = {0};
    size_t bytes_to_consume = strlen(encoded), bufsz, i, parsesz;
    ssize_t ret;
    const char *buf = encoded;

    dec.consume_trailer = consume_trailer;

    note("testing per-byte, source at line %d", line);

    bufsz = 0;
    for (i = 0; i < bytes_to_consume; ++i) {
        bufsz ++;
        ret = ho_parse_chunked(&dec, buf, bufsz, &parsesz);

        if (ret >= 0) {
            ok(expected_ret == i+1);
            ok(expected_rem == bufsz-parsesz);
            goto cleanup;
        } else if (ret == -1) {
            ok(expected_ret == -1);
            goto cleanup;
        }
        buf += parsesz;
        bufsz -= parsesz;
    }
    
    ok(expected_ret == -2);
    ok(expected_rem == bufsz);

cleanup:
    ;
}

static void test_chunked_failure(int line, const char *encoded, ssize_t expected_ret)
{
    struct phr_chunked_decoder dec = {0};
    char *buf = encoded;
    size_t bufsz, i, parse_sz;
    ssize_t ret;

    note("testing failure at-once, source at line %d", line);
    bufsz = strlen(buf);
    ret = ho_parse_chunked(&dec, buf, bufsz, NULL);
    ok(ret == expected_ret);

    note("testing failure per-byte, source at line %d", line);
    memset(&dec, 0, sizeof(dec));
    bufsz = 0;
    for (i = 0; encoded[i] != '\0'; ++i) {
        bufsz++;
        ret = ho_parse_chunked(&dec, buf, bufsz, &parse_sz);
        
        if (ret == -1) {
            ok(ret == expected_ret);
            goto cleanup;
        } else if (ret == -2) {
            /* continue */
        } else {
            ok(0);
            goto cleanup;
        }
        bufsz -= parse_sz;
        buf += parse_sz;
    }
    ok(ret == expected_ret);

cleanup:
    ;
}

static void (*chunked_test_runners[])(int, int, const char *, ssize_t, ssize_t) = {test_chunked_at_once,
                                                                                   test_chunked_per_byte,
                                                                                   NULL};

static void test_chunked(void)
{
    size_t i;
#define CRLF "\r\n"
    
    for (i = 0; chunked_test_runners[i] != NULL; ++i) {
        #define T "b\r\nhello world\r\n0\r\n"
        chunked_test_runners[i](__LINE__, 1, T, -2, 0);
        chunked_test_runners[i](__LINE__, 1, T CRLF, strlen(T)+2, 0);
        #undef T
        #define T "6\r\nhello \r\n5\r\nworld\r\n0\r\n"
        chunked_test_runners[i](__LINE__, 1, T, -2, 0);
        chunked_test_runners[i](__LINE__, 1, T CRLF, strlen(T)+2, 0);
        #undef T
        #define T "6;comment=hi\r\nhello \r\n5\r\nworld\r\n0\r\n"
        chunked_test_runners[i](__LINE__, 1, T, -2, 0);
        chunked_test_runners[i](__LINE__, 1, T CRLF, strlen(T)+2, 0);
        #undef T
        #define T "6\r\nhello \r\n5\r\nworld\r\n0\r\na: b\r\nc: d\r\n\r\n"
        chunked_test_runners[i](__LINE__, 1, T, strlen(T), 0);
        #undef T
        #define T "b\r\nhello world\r\n0\r\n"
        chunked_test_runners[i](__LINE__, 1, T, -2, 0);
        chunked_test_runners[i](__LINE__, 1, T CRLF, strlen(T)+2, 0);
        #undef T
    }

    note("failures");
    test_chunked_failure(__LINE__, "z\r\nabcdefg", -1);
    if (sizeof(size_t) == 8) {
        test_chunked_failure(__LINE__, "6\r\nhello \r\nffffffffffffffff\r\nabcdefg", -2);
        test_chunked_failure(__LINE__, "6\r\nhello \r\nfffffffffffffffff\r\nabcdefg", -1);
    }
}

int main(int argc, char **argv)
{
    subtest("chunked", test_chunked);
    return done_testing();
}

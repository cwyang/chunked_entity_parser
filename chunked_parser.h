/* set ts=4 sw=4 enc=utf-8: -*- Mode: c; tab-width: 4; c-basic-offset:4; coding: utf-8 -*- */
/*
 * HTTP chunked entity parser
 * 22 March 2019
 * Chul-Woong Yang (cwyang@gmail.com)
 *
 * This module just 'parses' chunked stream and 'detects' end position,
 * 
 * Use picohttpparser (https://github.com/h2o/picohttpparser)
 * if you want to decode chunked entity.
 */

#ifndef HO_CHUNKED_PARSER
#define HO_CHUNKED_PARSER
#include <assert.h>
#include <stdio.h>

#ifndef picohttpparser_h /* same structure of picohttpparser */
/* should be zero-filled before start */
struct phr_chunked_decoder {
    size_t bytes_left_in_chunk; /* number of bytes left in current chunk */
    char consume_trailer; /* if trailing headers should be consumed */
    char _hex_count;
    char _state;
};
#endif

/* The function scans the buffer given as (buf, bufsz) until it finds
 * the end of given chunked entity.
 * Applications should repeatedly call the function while it returns -2 (incomplete)
 * every time supplying newly arrived data.
 * bufsz is updated to parsed len. That is, buf should be set to `buf + bufsze` at next call.
 * When the end of the chunked-encoded data is found, the function returns 0.
 * Returns -1 on error.
 */
ssize_t ho_parse_chunked(struct phr_chunked_decoder *decoder, const char *buf,
                         size_t bufsz, size_t *parse_size);
#endif


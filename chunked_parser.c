/* set ts=4 sw=4 enc=utf-8: -*- Mode: c; tab-width: 4; c-basic-offset:4; coding: utf-8 -*- */
/*
 * HTTP chunked entity parser
 * 22 March 2019
 * Chul-Woong Yang (cwyang@gmail.com)
 *
 * This module just 'parses' chunked stream and 'detects' end position,
 * 
 * Use Kazuho's picohttpparser (https://github.com/h2o/picohttpparser)
 * if you want to decode chunked entity.
 * Kazuho's codes are very cool!
 */

#include "chunked_parser.h"

enum {
    CHUNKED_IN_CHUNK_SIZE,
    CHUNKED_IN_CHUNK_EXT,
    CHUNKED_IN_CHUNK_DATA,
    CHUNKED_IN_CHUNK_CRLF,
    CHUNKED_IN_TRAILERS_LINE_HEAD,
    CHUNKED_IN_TRAILERS_LINE_MIDDLE
};

static int decode_hex(int ch)
{
    if ('0' <= ch && ch <= '9') {
        return ch - '0';
    } else if ('A' <= ch && ch <= 'F') {
        return ch - 'A' + 0xa;
    } else if ('a' <= ch && ch <= 'f') {
        return ch - 'a' + 0xa;
    } else {
        return -1;
    }
}

ssize_t ho_parse_chunked(struct phr_chunked_decoder *decoder, const char *buf,
                         size_t bufsz, size_t *_parsesz)
{
    size_t parsesz = 0, src = 0;
    ssize_t ret = -2; /* incomplete */

    while (1) {
        switch (decoder->_state) {
        case CHUNKED_IN_CHUNK_SIZE:
            parsesz = src;
            for (; ; ++src) {
                int v;
                if (src == bufsz) {
                    parsesz = src;  // keep digit-read state
                    goto Exit;
                }
                if ((v = decode_hex(buf[src])) == -1) {
                    if (decoder->_hex_count == 0) {
                        ret = -1;
                        goto Exit;
                    }
                    break;
                }
                if (decoder->_hex_count == sizeof(size_t) * 2) {
                    ret = -1;
                    goto Exit;
                }
                decoder->bytes_left_in_chunk = decoder->bytes_left_in_chunk * 16 + v;
                ++decoder->_hex_count;
            }
            decoder->_hex_count = 0;
            decoder->_state = CHUNKED_IN_CHUNK_EXT;
            /* fallthru */
        case CHUNKED_IN_CHUNK_EXT:
            parsesz = src;
            /* RFC 7230 A.2 "Line folding in chunk extensions is disallowed" */
            for (; ; ++src) {
                if (src == bufsz)
                    goto Exit;
                if (buf[src] == '\012')
                    break;
            }
            ++src;
            if (decoder->bytes_left_in_chunk == 0) {
                if (decoder->consume_trailer) {
                    decoder->_state = CHUNKED_IN_TRAILERS_LINE_HEAD;
                    break;
                } else {
                    goto Complete;
                }
            }
            decoder->_state = CHUNKED_IN_CHUNK_DATA;
            /* fallthru */
        case CHUNKED_IN_CHUNK_DATA:
        {
            size_t avail = bufsz - src;
            parsesz = src;
            if (avail < decoder->bytes_left_in_chunk) {
                src += avail;
                parsesz = src;
                decoder->bytes_left_in_chunk -= avail;
                goto Exit;
            }
            src += decoder->bytes_left_in_chunk;
            decoder->bytes_left_in_chunk = 0;
            decoder->_state = CHUNKED_IN_CHUNK_CRLF;
        }
        /* fallthru */
        case CHUNKED_IN_CHUNK_CRLF:
            parsesz = src;
            for (; ; ++src) {
                if (src == bufsz)
                    goto Exit;
                if (buf[src] != '\015')
                    break;
            }
            if (buf[src] != '\012') {
                ret = -1;
                goto Exit;
            }
            ++src;
            decoder->_state = CHUNKED_IN_CHUNK_SIZE;
            break;
        case CHUNKED_IN_TRAILERS_LINE_HEAD:
            parsesz = src;
            for (; ; ++src) {
                if (src == bufsz)
                    goto Exit;
                if (buf[src] != '\015')
                    break;
            }
            if (buf[src++] == '\012')
                goto Complete;
            decoder->_state = CHUNKED_IN_TRAILERS_LINE_MIDDLE;
            /* fallthru */
        case CHUNKED_IN_TRAILERS_LINE_MIDDLE:
            parsesz = src;
            for (; ; ++src) {
                if (src == bufsz)
                    goto Exit;
                if (buf[src] == '\012')
                    break;
            }
            ++src;
            decoder->_state = CHUNKED_IN_TRAILERS_LINE_HEAD;
            break;
        default:
            assert(!"decoder is corrupt");
        }
    }

Complete:
    ret = parsesz = src;
Exit:
    if (_parsesz)
        *_parsesz = parsesz;
    
    return ret;
}


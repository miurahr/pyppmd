//
// Created by miurahr on 2021/08/07.
//

#include "Ppmd8Tdecoder.h"

PPMD_pthread_mutex_t ppmd8mutex = PPMD_PTHREAD_MUTEX_INITIALIZER;
PPMD_pthread_cond_t ppmd8NotEmpty = PPMD_PTHREAD_COND_INITIALIZER;
PPMD_pthread_cond_t ppmd8InEmpty = PPMD_PTHREAD_COND_INITIALIZER;

Byte Ppmd8Reader(const void *p) {
    BufferReader *bufferReader = (BufferReader *)p;
    if (bufferReader->inBuffer->pos == bufferReader->inBuffer->size) {
        PPMD_pthread_mutex_lock(&ppmd8mutex);
        PPMD_pthread_cond_broadcast(&ppmd8InEmpty);
        while (bufferReader->inBuffer->pos == bufferReader->inBuffer->size) {
            PPMD_pthread_cond_wait(&ppmd8NotEmpty, &ppmd8mutex);
        }
        PPMD_pthread_mutex_unlock(&ppmd8mutex);
    }
    return *((const Byte *)bufferReader->inBuffer->src + bufferReader->inBuffer->pos++);
}

static void *
Ppmd8T_decode_run(void *p) {
    ppmd8_args *args = (ppmd8_args *)p;
    args->finished = False;
    CPpmd8 * cPpmd8 = args->cPpmd8;
    BufferReader *reader = (BufferReader *) cPpmd8->Stream.In;
    int max_length = args->max_length;

    Bool escaped = False;
    int i = 0;
    int result;
    while (i < max_length ) {
        Bool inbuf_empty = reader->inBuffer->size == reader->inBuffer->pos;
        Bool outbuf_full = args->out->size == args->out->pos;
        if (inbuf_empty || outbuf_full) {
            break;
        }
        int c = Ppmd8_DecodeSymbol(cPpmd8);
        if (c == PPMD8_RESULT_EOF) {
            result = PPMD8_RESULT_EOF;
            goto exit;
        } else if (c == PPMD8_RESULT_ERROR) {
            result = PPMD8_RESULT_ERROR;
            goto exit;
        }
        if (args->endmark) {
            if (escaped) {
                escaped = False;
                if (c == 0x01) { // escaped character
                    PPMD_pthread_mutex_lock(&ppmd8mutex);
                    *((Byte *)args->out->dst + args->out->pos++) = c;
                    PPMD_pthread_mutex_unlock(&ppmd8mutex);
                    i++;
                } else if (c == 0x00) { // endmark
                    // eof
                    result = PPMD8_RESULT_EOF;
                    goto exit;
                } else {
                    // failed
                    result = PPMD8_RESULT_ERROR;
                    goto exit;
                }
            } else {
                if (c != 0x01) { // ordinary data
                    PPMD_pthread_mutex_lock(&ppmd8mutex);
                    *((Byte *)args->out->dst + args->out->pos++) = (Byte) c;
                    PPMD_pthread_mutex_unlock(&ppmd8mutex);
                    i++;
                } else { // enter escape sequence
                    escaped = True;
                }
            }
        } else {
            PPMD_pthread_mutex_lock(&ppmd8mutex);
            *((Byte *)args->out->dst + args->out->pos++) = (Byte) c;
            PPMD_pthread_mutex_unlock(&ppmd8mutex);
            i++;
        }
    }
    // when success return produced size
    result = i;

    exit:
    PPMD_pthread_mutex_lock(&ppmd8mutex);
    args->result = result;
    args->finished = True;
    PPMD_pthread_mutex_unlock(&ppmd8mutex);
    return NULL;
}

int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd8_args *args) {
    PPMD_pthread_mutex_lock(&ppmd8mutex);
    args->cPpmd8 = cPpmd8;
    args->max_length = max_length;
    args->out = out;
    BufferReader *reader = (BufferReader *) cPpmd8->Stream.In;
    args->result = 0;
    Bool exited = args->finished;
    args->finished = False;
    PPMD_pthread_mutex_unlock(&ppmd8mutex);

    if (exited) {
        PPMD_pthread_create(&(args->handle), NULL, Ppmd8T_decode_run, args);
        PPMD_pthread_mutex_lock(&ppmd8mutex);
        PPMD_pthread_mutex_unlock(&ppmd8mutex);
    } else {
        PPMD_pthread_mutex_lock(&ppmd8mutex);
        if (reader->inBuffer->pos < reader->inBuffer->size) {
            PPMD_pthread_cond_broadcast(&ppmd8NotEmpty);
            PPMD_pthread_mutex_unlock(&ppmd8mutex);
        } else {
            PPMD_pthread_mutex_unlock(&ppmd8mutex);
            PPMD_pthread_cancel(args->handle);
            args->finished = True;
            return PPMD8_RESULT_ERROR;  // error
        }
    }
    PPMD_pthread_mutex_lock(&ppmd8mutex);
    unsigned long wait = 50000;
    while(True) {
        PPMD_pthread_cond_timedwait(&ppmd8InEmpty, &ppmd8mutex, wait);
        if (args->finished) {
            // when finished, the input buffer will be empty,
            // so check finished status before checking buffer.
            goto finished;
        }
        if (reader->inBuffer->pos == reader->inBuffer->size) {
            goto inempty;
        }
    }
finished:
    PPMD_pthread_mutex_unlock(&ppmd8mutex);
    PPMD_pthread_join(args->handle, NULL);
    return args->result;

inempty:
    PPMD_pthread_mutex_unlock(&ppmd8mutex);
    return 0;
}

void Ppmd8T_Free(CPpmd8 *cPpmd8, ppmd8_args *args, ISzAllocPtr allocator) {
    if (!(args->finished)) {
        PPMD_pthread_cancel(args->handle);
        args->finished = True;
    }
    Ppmd8_Free(cPpmd8, allocator);
}
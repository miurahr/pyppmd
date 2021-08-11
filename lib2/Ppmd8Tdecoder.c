//
// Created by miurahr on 2021/08/07.
//

#include "Ppmd8Tdecoder.h"

PPMD_pthread_mutex_t mutex = PPMD_PTHREAD_MUTEX_INITIALIZER;
PPMD_pthread_cond_t notEmpty = PPMD_PTHREAD_COND_INITIALIZER;
PPMD_pthread_cond_t inEmpty = PPMD_PTHREAD_COND_INITIALIZER;

Byte TReader(const void *p) {
    BufferReader *bufferReader = (BufferReader *)p;
    while (bufferReader->inBuffer->pos == bufferReader->inBuffer->size) {
        PPMD_pthread_mutex_lock(&mutex);
        PPMD_pthread_cond_signal(&inEmpty);
        PPMD_pthread_cond_wait(&notEmpty, &mutex);
        PPMD_pthread_mutex_unlock(&mutex);
    }
    return *((const Byte *)bufferReader->inBuffer->src + bufferReader->inBuffer->pos++);
}

Bool Ppmd8T_decode_init() {
    return True;
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
                    PPMD_pthread_mutex_lock(&mutex);
                    *((Byte *)args->out->dst + args->out->pos++) = c;
                    PPMD_pthread_mutex_unlock(&mutex);
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
                    PPMD_pthread_mutex_lock(&mutex);
                    *((Byte *)args->out->dst + args->out->pos++) = (Byte) c;
                    PPMD_pthread_mutex_unlock(&mutex);
                    i++;
                } else { // enter escape sequence
                    escaped = True;
                }
            }
        } else {
            PPMD_pthread_mutex_lock(&mutex);
            *((Byte *)args->out->dst + args->out->pos++) = (Byte) c;
            PPMD_pthread_mutex_unlock(&mutex);
            i++;
        }
    }
    // when success return produced size
    result = i;

    exit:
    PPMD_pthread_mutex_lock(&mutex);
    args->result = result;
    args->finished = True;
    PPMD_pthread_mutex_unlock(&mutex);
    return NULL;
}

int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd8_args *args) {
    PPMD_pthread_mutex_lock(&mutex);
    args->cPpmd8 = cPpmd8;
    args->max_length = max_length;
    args->out = out;
    BufferReader *reader = (BufferReader *) cPpmd8->Stream.In;
    args->result = 0;
    Bool exited = args->finished;
    PPMD_pthread_mutex_unlock(&mutex);

    if (exited) {
        PPMD_pthread_t handle;
        PPMD_pthread_create(&handle, NULL, Ppmd8T_decode_run, args);
        PPMD_pthread_mutex_lock(&mutex);
        args->handle = handle;
        PPMD_pthread_mutex_unlock(&mutex);
    } else {
        PPMD_pthread_mutex_lock(&mutex);
        if (reader->inBuffer->pos < reader->inBuffer->size) {
            PPMD_pthread_cond_signal(&notEmpty);
            PPMD_pthread_mutex_unlock(&mutex);
        } else {
            PPMD_pthread_mutex_unlock(&mutex);
            return PPMD8_RESULT_ERROR;  // error
        }
    }
    PPMD_pthread_mutex_lock(&mutex);
    while(True) {
        if (PPMD_pthread_cond_wait1(&inEmpty, &mutex) == 0) {
            if (reader->inBuffer->pos == reader->inBuffer->size) {
                goto inempty;
            }
        }
        if (args->finished) {
            break;
        }
    }
    PPMD_pthread_mutex_unlock(&mutex);
    PPMD_pthread_join(args->handle, NULL);
    return args->result;

inempty:
    PPMD_pthread_mutex_unlock(&mutex);
    return 0;
}

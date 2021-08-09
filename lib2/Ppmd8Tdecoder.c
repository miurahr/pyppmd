//
// Created by miurahr on 2021/08/07.
//

#include "Ppmd8Tdecoder.h"

PPMD_pthread_mutex_t mutex = PPMD_PTHREAD_MUTEX_INITIALIZER;
PPMD_pthread_cond_t notEmpty = PPMD_PTHREAD_COND_INITIALIZER;
PPMD_pthread_cond_t inEmpty = PPMD_PTHREAD_COND_INITIALIZER;

Byte TReader(const void *p) {
    BufferReader *bufferReader = (BufferReader *)p;
    if (bufferReader->inBuffer->pos == bufferReader->inBuffer->size) {
        PPMD_pthread_cond_signal(&inEmpty);
        PPMD_pthread_cond_wait(&notEmpty, &mutex);
    }
    return *((const Byte *)bufferReader->inBuffer->src + bufferReader->inBuffer->pos++);
}

Bool Ppmd8T_decode_init() {
    return True;
}

static void *
Ppmd8T_decode_run(void *p) {
    ppmd8_args *args = (ppmd8_args *)p;
    PPMD_pthread_mutex_lock(&mutex);
    CPpmd8 * cPpmd8 = args->cPpmd8;
    BufferReader *reader = (BufferReader *) cPpmd8->Stream.In;
    int max_length = args->max_length;
    args->finished = False;
    PPMD_pthread_mutex_unlock(&mutex);

    Bool escaped = False;
    int i = 0;
    int result;
    while (i < max_length ) {
        PPMD_pthread_mutex_lock(&mutex);
        Bool inbuf_empty = reader->inBuffer->size == reader->inBuffer->pos;
        Bool outbuf_full = args->out->size == args->out->pos;
        PPMD_pthread_mutex_unlock(&mutex);
        if (inbuf_empty || outbuf_full) {
            break;
        }
        PPMD_pthread_mutex_lock(&mutex);
        int c = Ppmd8_DecodeSymbol(cPpmd8);
        PPMD_pthread_mutex_unlock(&mutex);
        if (c == PPMD8_RESULT_EOF) {
            PPMD_pthread_mutex_lock(&mutex);
            result = PPMD8_RESULT_EOF;
            PPMD_pthread_mutex_unlock(&mutex);
            goto exit;
        } else if (c == PPMD8_RESULT_ERROR) {
            PPMD_pthread_mutex_lock(&mutex);
            result = PPMD8_RESULT_ERROR;
            PPMD_pthread_mutex_unlock(&mutex);
            goto exit;
        }
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
    InBuffer *in = reader->inBuffer;
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
        if (in->pos < in->size) {
            PPMD_pthread_cond_signal(&notEmpty);
            PPMD_pthread_mutex_unlock(&mutex);
        } else {
            PPMD_pthread_mutex_unlock(&mutex);
            return -2;  // error
        }
    }
    while(True) {
        PPMD_pthread_mutex_lock(&mutex);
        if (PPMD_pthread_cond_wait1(&inEmpty, &mutex) == 0) {
            // inBuffer is empty
            PPMD_pthread_mutex_unlock(&mutex);
            return 0;
        }
        if (args->finished) {
            PPMD_pthread_mutex_unlock(&mutex);
            break;
        }
        PPMD_pthread_mutex_unlock(&mutex);
    }
    PPMD_pthread_join(args->handle, NULL);
    return args->result;
}

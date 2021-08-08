//
// Created by miurahr on 2021/08/07.
//

#include "Ppmd8Tdecoder.h"

PPMD_pthread_mutex_t mutex;
PPMD_pthread_cond_t finished, inEmpty, notEmpty;

Byte TReader(const void *p) {
    BufferReader *bufferReader = (BufferReader *)p;
    if (bufferReader->inBuffer->pos == bufferReader->inBuffer->size) {
        PPMD_pthread_cond_signal(&inEmpty);
        PPMD_pthread_cond_wait(&notEmpty, &mutex);
    }
    return *((const Byte *)bufferReader->inBuffer->src + bufferReader->inBuffer->pos++);
}

Bool Ppmd8T_decode_init() {
    PPMD_pthread_mutex_init(&mutex, NULL);
    PPMD_pthread_cond_init(&finished, NULL);
    PPMD_pthread_cond_init(&inEmpty, NULL);
    PPMD_pthread_cond_init(&notEmpty, NULL);
    return True;
}

static void *
Ppmd8T_decode_run(void *p) {
    ppmd8_args *args = (ppmd8_args *)p;
    PPMD_pthread_mutex_lock(&mutex);
    CPpmd8 * cPpmd8 = args->cPpmd8;
    BufferReader *reader = (BufferReader *) cPpmd8->Stream.In;
    InBuffer *in = reader->inBuffer;
    int max_length = args->max_length;
    args->finished = False;
    PPMD_pthread_mutex_unlock(&mutex);
    int i = 0;
    int result = 0;
    while (i < max_length ) {
        Bool can_break = False;
        PPMD_pthread_mutex_lock(&mutex);
        //if (in->size == in->pos) {
        //    can_break = True;
        //}
        if (args->out->size == args->out->pos) {
            can_break = True;
        }
        PPMD_pthread_mutex_unlock(&mutex);
        if (can_break) {
            break;
        }
        PPMD_pthread_mutex_lock(&mutex);
        int c = Ppmd8_DecodeSymbol(cPpmd8);
        PPMD_pthread_mutex_unlock(&mutex);
        if (c < 0) {
            result = c;
            goto exit;
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
    InBuffer *in = reader->inBuffer;
    args->in = in;
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
        if (!args->result) { //in->pos < in->size) {
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

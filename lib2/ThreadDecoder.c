//
// Created by miurahr on 2021/08/07.
//

#include "ThreadDecoder.h"
#include "Buffer.h"

Byte Ppmd8Reader(const void *p) {
    BufferReader *bufferReader = (BufferReader *)p;
    ppmd_thread_info *t = bufferReader->t;
    InBuffer *inBuffer = t->in;
    if (inBuffer->pos == inBuffer->size) {
        PPMD_pthread_mutex_lock(&t->mutex);
        PPMD_pthread_cond_broadcast(&t->inEmpty);
        while (inBuffer->pos == inBuffer->size) {
            PPMD_pthread_cond_wait(&t->notEmpty, &t->mutex);
        }
        PPMD_pthread_mutex_unlock(&t->mutex);
    }
    return *((const Byte *)inBuffer->src + inBuffer->pos++);
}

void Ppmd_thread_decode_init(ppmd_thread_info *t) {
    PPMD_pthread_mutex_init(&t->mutex, NULL);
    PPMD_pthread_cond_init(&t->inEmpty, NULL);
    PPMD_pthread_cond_init(&t->notEmpty, NULL);
}

static void *
Ppmd8T_decode_run(void *p) {
    ppmd_thread_info *threadInfo = (ppmd_thread_info *)p;
    threadInfo->finished = False;
    CPpmd8 * cPpmd8 = (CPpmd8 *)(threadInfo->cPpmd);
    BufferReader *reader = (BufferReader *) cPpmd8->Stream.In;
    int max_length = threadInfo->max_length;

    Bool escaped = False;
    int i = 0;
    int result;
    while (i < max_length ) {
        Bool inbuf_empty = reader->inBuffer->size == reader->inBuffer->pos;
        Bool outbuf_full = threadInfo->out->size == threadInfo->out->pos;
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
        if (threadInfo->endmark) {
            if (escaped) {
                escaped = False;
                if (c == 0x01) { // escaped character
                    PPMD_pthread_mutex_lock(&threadInfo->mutex);
                    *((Byte *)threadInfo->out->dst + threadInfo->out->pos++) = c;
                    PPMD_pthread_mutex_unlock(&threadInfo->mutex);
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
                    PPMD_pthread_mutex_lock(&threadInfo->mutex);
                    *((Byte *)threadInfo->out->dst + threadInfo->out->pos++) = (Byte) c;
                    PPMD_pthread_mutex_unlock(&threadInfo->mutex);
                    i++;
                } else { // enter escape sequence
                    escaped = True;
                }
            }
        } else {
            PPMD_pthread_mutex_lock(&threadInfo->mutex);
            *((Byte *)threadInfo->out->dst + threadInfo->out->pos++) = (Byte) c;
            PPMD_pthread_mutex_unlock(&threadInfo->mutex);
            i++;
        }
    }
    // when success return produced size
    result = i;

    exit:
    PPMD_pthread_mutex_lock(&threadInfo->mutex);
    threadInfo->result = result;
    threadInfo->finished = True;
    PPMD_pthread_mutex_unlock(&threadInfo->mutex);
    return NULL;
}

int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd_thread_info *threadInfo) {
    PPMD_pthread_mutex_lock(&threadInfo->mutex);
    BufferReader *reader = (BufferReader *) cPpmd8->Stream.In;
    threadInfo->cPpmd = (void *) cPpmd8;
    threadInfo->max_length = max_length;
    threadInfo->out = out;
    threadInfo->result = 0;
    Bool exited = threadInfo->finished;
    threadInfo->finished = False;
    PPMD_pthread_mutex_unlock(&threadInfo->mutex);

    if (exited) {
        PPMD_pthread_create(&(threadInfo->handle), NULL, Ppmd8T_decode_run, threadInfo);
        PPMD_pthread_mutex_lock(&threadInfo->mutex);
        PPMD_pthread_mutex_unlock(&threadInfo->mutex);
    } else {
        PPMD_pthread_mutex_lock(&threadInfo->mutex);
        if (reader->inBuffer->pos < reader->inBuffer->size) {
            PPMD_pthread_cond_broadcast(&threadInfo->notEmpty);
            PPMD_pthread_mutex_unlock(&threadInfo->mutex);
        } else {
            PPMD_pthread_mutex_unlock(&threadInfo->mutex);
            PPMD_pthread_cancel(threadInfo->handle);
            threadInfo->finished = True;
            return PPMD8_RESULT_ERROR;  // error
        }
    }
    PPMD_pthread_mutex_lock(&threadInfo->mutex);
    unsigned long wait = 50000;
    while(True) {
        PPMD_pthread_cond_timedwait(&threadInfo->inEmpty, &threadInfo->mutex, wait);
        if (threadInfo->finished) {
            // when finished, the input buffer will be empty,
            // so check finished status before checking buffer.
            goto finished;
        }
        if (reader->inBuffer->pos == reader->inBuffer->size) {
            goto inempty;
        }
    }
finished:
    PPMD_pthread_mutex_unlock(&threadInfo->mutex);
    PPMD_pthread_join(threadInfo->handle, NULL);
    return threadInfo->result;

inempty:
    PPMD_pthread_mutex_unlock(&threadInfo->mutex);
    return 0;
}

void Ppmd8T_Free(CPpmd8 *cPpmd8, ppmd_thread_info *args, ISzAllocPtr allocator) {
    if (!(args->finished)) {
        PPMD_pthread_cancel(args->handle);
        args->finished = True;
    }
    Ppmd8_Free(cPpmd8, allocator);
}
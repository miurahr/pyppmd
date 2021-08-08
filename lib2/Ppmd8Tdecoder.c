//
// Created by miurahr on 2021/08/07.
//

#include <signal.h>
#include "Ppmd8Tdecoder.h"

PPMD_pthread_mutex_t mutex = PPMD_PTHREAD_MUTEX_INITIALIZER;
PPMD_pthread_cond_t notEmpty = PPMD_PTHREAD_COND_INITIALIZER;

/**
 * Read a char from inBuffer in thread context.
 * it should be called with mutex locked.
 * @param p IByteIn * (casted to BufferReader *)
 * @return char
 */
Byte TReader(const void *p) {
    BufferReader *bufferReader = (BufferReader *)p;
    if (bufferReader->inBuffer->pos == bufferReader->inBuffer->size) {
        // inbuffer is exhausted, so wait to refill it.
        PPMD_pthread_cond_wait(&notEmpty, &mutex);
        // An address of bufferReader->inBuffer may be changed here.
        // so should not reuse it.
    }
    return *((const Byte *)bufferReader->inBuffer->src + bufferReader->inBuffer->pos++);
}

static void *
Ppmd8T_decode_run(void *p) {
    ppmd8_decode_status *statusS = (ppmd8_decode_status *)p;
    PPMD_pthread_mutex_lock(&mutex);
    CPpmd8 * cPpmd8 = statusS->cPpmd8;
    BufferReader *reader = (BufferReader *) cPpmd8->Stream.In;
    int max_length = statusS->max_length;
    statusS->finished = False;
    PPMD_pthread_mutex_unlock(&mutex);

    Bool escaped = False;
    Bool isFull = False;
    int i = 0;
    while (i < max_length ) {
        // DecodeSymbol called under mutex locked
        PPMD_pthread_mutex_lock(&mutex);
        int c = Ppmd8_DecodeSymbol(cPpmd8);
        Bool isEmpty = reader->inBuffer->size == reader->inBuffer->pos;
        if (isEmpty) statusS->finished = True;
        PPMD_pthread_mutex_unlock(&mutex);
        if (c == PPMD8_RESULT_EOF) {
            PPMD_pthread_mutex_lock(&mutex);
            statusS->result = PPMD8_RESULT_EOF;
            statusS->finished = True;
            PPMD_pthread_mutex_unlock(&mutex);
            break;
        } else if (c == PPMD8_RESULT_ERROR) {
            PPMD_pthread_mutex_lock(&mutex);
            statusS->result = PPMD8_RESULT_ERROR;
            statusS->finished = True;
            PPMD_pthread_mutex_unlock(&mutex);
            break;
        }
        if (escaped) {
            escaped = False;
            if (c == 0x01) { // escaped character
                PPMD_pthread_mutex_lock(&mutex);
                *((Byte *)statusS->out->dst + statusS->out->pos++) = c;
                isFull = statusS->out->size == statusS->out->pos;
                PPMD_pthread_mutex_unlock(&mutex);
                i++;
            } else if (c == 0x00) { // endmark
                PPMD_pthread_mutex_lock(&mutex);
                statusS->result = PPMD8_RESULT_EOF;
                statusS->finished = True;
                PPMD_pthread_mutex_unlock(&mutex);
                break;
            } else {
                PPMD_pthread_mutex_lock(&mutex);
                statusS->result = PPMD8_RESULT_ERROR;
                statusS->finished = True;
                PPMD_pthread_mutex_unlock(&mutex);
                break;
            }
        } else {
            if (c != 0x01) { // ordinary data
                PPMD_pthread_mutex_lock(&mutex);
                *((Byte *)statusS->out->dst + statusS->out->pos++) = (Byte) c;
                isFull = statusS->out->size == statusS->out->pos;
                PPMD_pthread_mutex_unlock(&mutex);
                i++;
            } else { // enter escape sequence
                escaped = True;
            }
        }
        if (isEmpty || isFull) {
            PPMD_pthread_mutex_lock(&mutex);
            statusS->result = i;
            statusS->finished = True;
            PPMD_pthread_mutex_unlock(&mutex);
            break;
        }
    }
    return NULL;
}

int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd8_decode_status *status) {
    PPMD_pthread_mutex_lock(&mutex);
    status->cPpmd8 = cPpmd8;
    status->max_length = max_length;
    status->out = out;
    status->result = 0;
    Bool finished = status->finished;
    PPMD_pthread_mutex_unlock(&mutex);
    Bool isEmpty = False;

    if (finished) {
        PPMD_pthread_t handle;
        PPMD_pthread_create(&handle, NULL, Ppmd8T_decode_run, status);
        PPMD_pthread_mutex_lock(&mutex);
        status->handle = handle;
        PPMD_pthread_mutex_unlock(&mutex);
        finished = False;
    } else {
        PPMD_pthread_mutex_lock(&mutex);
        BufferReader *reader = (BufferReader *) status->cPpmd8->Stream.In;
        if (reader->inBuffer->pos < reader->inBuffer->size) {
            // The thread is waiting input data
            PPMD_pthread_cond_signal(&notEmpty);
        } else { // there is no data
            PPMD_pthread_mutex_unlock(&mutex);
            return PPMD8_RESULT_ERROR;  // error
        }
        PPMD_pthread_mutex_unlock(&mutex);
    }
    int i = 0;
    while(!finished) {
        if (isEmpty) { // inBuffer is empty but not finished, ask next data
            return 0;
        }
        if (i++ > 500000) {  // too many wait for single call(5sec) -> timeout and abort
            raise (SIGABRT);
        }
        PPMD_100nanosleep(100); // sleep 100 * 100ns
        PPMD_pthread_mutex_lock(&mutex);
        // We should get statuses in atomic
        finished = status->finished;
        BufferReader *reader = (BufferReader *) status->cPpmd8->Stream.In;
        isEmpty = (reader->inBuffer->pos == reader->inBuffer->size);
        PPMD_pthread_mutex_unlock(&mutex);
    }
    PPMD_pthread_join(status->handle, NULL);
    return status->result;
}

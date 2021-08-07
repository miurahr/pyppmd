//
// Created by miurahr on 2021/08/07.
//

#include "Ppmd8Tdecoder.h"

pthread_mutex_t mutex;
pthread_cond_t finished, outFull, inEmpty, notEmpty;

Byte TReader(const void *p) {
    BufferReader *bufferReader = (BufferReader *)p;
    if (bufferReader->inBuffer->pos == bufferReader->inBuffer->size) {
        pthread_cond_signal(&inEmpty);
        pthread_cond_wait(&notEmpty, &mutex);
    }
    return *((const Byte *)bufferReader->inBuffer->src + bufferReader->inBuffer->pos++);
}

Bool Ppmd8T_decode_init(CPpmd8 *cPpmd8, UInt32 memory_size, unsigned int maximum_order, ISzAlloc *allocator) {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&finished, NULL);
    pthread_cond_init(&outFull, NULL);
    pthread_cond_init(&inEmpty, NULL);
    pthread_cond_init(&notEmpty, NULL);

    Ppmd8_Construct(cPpmd8);
    if (Ppmd8_Alloc(cPpmd8, memory_size, allocator)) {
        Ppmd8_Init(cPpmd8, maximum_order, PPMD8_RESTORE_METHOD_RESTART);
        return True;
    }
    return False;
}

static void *
Ppmd8T_decode_run(void *p) {
    ppmd8_args *args = (ppmd8_args *)p;
    pthread_mutex_lock(&mutex);
    CPpmd8 * cPpmd8 = args->cPpmd8;
    BufferReader *reader = (BufferReader *) cPpmd8->Stream.In;
    InBuffer *in = reader->inBuffer;
    int max_length = args->max_length;
    args->finished = False;
    pthread_mutex_unlock(&mutex);

    Bool escaped = False;
    int i = 0;
    int result = 0;
    while (i < max_length ) {
        Bool can_break = False;
        pthread_mutex_lock(&mutex);
        if (in->size == in->pos) {
            can_break = True;
        }
        if (args->out->size == args->out->pos) {
            can_break = True;
        }
        pthread_mutex_unlock(&mutex);
        if (can_break) {
            break;
        }
        pthread_mutex_lock(&mutex);
        int c = Ppmd8_DecodeSymbol(cPpmd8);
        pthread_mutex_unlock(&mutex);
        if (escaped) {
            escaped = False;
            if (c == 0x01) { // escaped character
                pthread_mutex_lock(&mutex);
                *((Byte *)args->out->dst + args->out->pos++) = c;
                pthread_mutex_unlock(&mutex);
                i++;
            } else if (c == 0x00) { // endmark
                // eof
                result = -2;
                goto exit;
            } else {
                // failed
                result = -1;
                goto exit;
            }
        } else {
            if (c != 0x01) { // ordinary data
                pthread_mutex_lock(&mutex);
                *((Byte *)args->out->dst + args->out->pos++) = (Byte) c;
                pthread_mutex_unlock(&mutex);
                i++;
            } else { // enter escape sequence
                escaped = True;
            }
        }
    }
    // when success return produced size
    result = i;

    exit:
    pthread_mutex_lock(&mutex);
    args->result = result;
    args->finished = True;
    pthread_mutex_unlock(&mutex);
    return NULL;
}

static void
get_wait(struct timespec *ts) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long tv_nsec = now.tv_nsec + 50000;
    if (tv_nsec >= 1000000000) {
        ts->tv_nsec = tv_nsec - 1000000000;
        ts->tv_sec = now.tv_sec + 1;
    } else {
        ts->tv_nsec = tv_nsec;
        ts->tv_sec = now.tv_sec;
    }
}

int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd8_args *args) {
    struct timespec ts;
    pthread_mutex_lock(&mutex);
    args->cPpmd8 = cPpmd8;
    args->max_length = max_length;
    args->out = out;
    BufferReader *reader = (BufferReader *) cPpmd8->Stream.In;
    InBuffer *in = reader->inBuffer;
    args->in = in;
    args->result = 0;
    Bool exited = args->finished;
    pthread_mutex_unlock(&mutex);

    if (exited) {
        pthread_t handle;
        pthread_create(&handle, NULL, Ppmd8T_decode_run, args);
        pthread_mutex_lock(&mutex);
        args->handle = handle;
        pthread_mutex_unlock(&mutex);
    } else {
        pthread_mutex_lock(&mutex);
        if (in->pos < in->size) {
            pthread_cond_signal(&notEmpty);
            pthread_mutex_unlock(&mutex);
        } else {
            pthread_mutex_unlock(&mutex);
            return -1;  // error
        }
    }
    while(True) {
        pthread_mutex_lock(&mutex);
        get_wait(&ts);
        if (pthread_cond_timedwait(&inEmpty, &mutex, &ts) == 0) {
            // inBuffer is empty
            pthread_mutex_unlock(&mutex);
            return 0;
        }
        if (args->finished) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
    }
    pthread_join(args->handle, NULL);
    return args->result;
}

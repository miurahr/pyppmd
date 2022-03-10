//
// Created by miurahr on 2021/08/07.
//

#ifndef PYPPMD_THREADDECODER_H
#define PYPPMD_THREADDECODER_H

#include "Buffer.h"

#ifdef _MSC_VER
#include "win_pthreads.h"
#else
#include <pthread.h>
#endif

#define PPMD_RESULT_EOF (-1)
#define PPMD_RESULT_ERROR (-2)

typedef struct ppmd_thread_control_s {
    pthread_t handle;
    pthread_mutex_t mutex;
    pthread_cond_t inEmpty;
    pthread_cond_t notEmpty;
    Bool empty;
    Bool finished;
} ppmd_thread_control_t;

Byte Ppmd_thread_Reader(const void *p);

int Ppmd7T_decode(CPpmd7 *cPpmd7, CPpmd7z_RangeDec *rc, OutBuffer *out, int max_length, ppmd_info *threadInfo);
void Ppmd7T_Free(CPpmd7 *cPpmd7, ppmd_info *args, IAllocPtr allocator);

int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd_info *threadInfo);
void Ppmd8T_Free(CPpmd8 *cPpmd8, ppmd_info *args, IAllocPtr allocator);

Bool Ppmd_thread_decode_init(ppmd_info *t, IAllocPtr allocator);

#endif //PYPPMD_THREADDECODER_H

//
// Created by miurahr on 2021/08/07.
//

#ifndef PYPPMD_THREADDECODER_H
#define PYPPMD_THREADDECODER_H

#include "Buffer.h"
#include "threading.h"

#define PPMD8_RESULT_EOF (-1)
#define PPMD8_RESULT_ERROR (-2)

typedef struct ppmd_thread_control_s {
    pthread_t handle;
    pthread_mutex_t mutex;
    pthread_cond_t inEmpty;
    pthread_cond_t notEmpty;
} ppmd_thread_control_t;

Byte Ppmd_thread_Reader(const void *p);
int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd_info *threadInfo);
void Ppmd8T_Free(CPpmd8 *cPpmd8, ppmd_info *args, ISzAllocPtr allocator);
Bool Ppmd_thread_decode_init(ppmd_info *t, ISzAllocPtr allocator);

#endif //PYPPMD_THREADDECODER_H

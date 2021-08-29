//
// Created by miurahr on 2021/08/07.
//

#ifndef PYPPMD_PPMD8TDECODER_H
#define PYPPMD_PPMD8TDECODER_H

#include "Buffer.h"
#include "threading.h"

typedef struct ppmd8_args_s {
    CPpmd8 *cPpmd8;
    OutBuffer *out;
    int max_length;
    Bool endmark;
    Bool finished;
    int result;
    PPMD_pthread_t handle;
} ppmd8_args;

#define PPMD8_RESULT_EOF (-1)
#define PPMD8_RESULT_ERROR (-2)

Byte Ppmd8Reader(const void *p);
int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd8_args *args);
void Ppmd8T_Free(CPpmd8 *cPpmd8, ppmd8_args *args, ISzAllocPtr allocator);

#endif //PYPPMD_PPMD8TDECODER_H

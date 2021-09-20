//
// Created by miurahr on 2021/08/07.
//

#ifndef PYPPMD_THREADDECODER_H
#define PYPPMD_THREADDECODER_H

#include "Buffer.h"
#include "threading.h"

#define PPMD8_RESULT_EOF (-1)
#define PPMD8_RESULT_ERROR (-2)

Byte Ppmd8Reader(const void *p);
int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd_thread_info *threadInfo);
void Ppmd8T_Free(CPpmd8 *cPpmd8, ppmd_thread_info *args, ISzAllocPtr allocator);
void Ppmd_thread_decode_init(ppmd_thread_info *t);

#endif //PYPPMD_THREADDECODER_H

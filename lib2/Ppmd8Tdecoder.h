//
// Created by miurahr on 2021/08/07.
//

#ifndef PYPPMD_PPMD8TDECODER_H
#define PYPPMD_PPMD8TDECODER_H

#include "Buffer.h"
#include "threading.h"

#define PPMD8_RESULT_EOF (-1)
#define PPMD8_RESULT_ERROR (-2)

typedef struct ppmd8_decode_status_s {
    CPpmd8 *cPpmd8;
    OutBuffer *out;
    int max_length;
    Bool finished;
    int result;
    PPMD_pthread_t handle;
} ppmd8_decode_status;


Byte TReader(const void *p);
int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd8_decode_status *status);

#endif //PYPPMD_PPMD8TDECODER_H

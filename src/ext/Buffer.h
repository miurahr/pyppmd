//
// Created by miurahr on 2021/08/06.
//

#ifndef PYPPMD_BUFFER_H
#define PYPPMD_BUFFER_H

#include "Ppmd8.h"

typedef struct InBuffer_s {
    const void* src;    /**< start of input buffer */
    size_t size;        /**< size of input buffer */
    size_t pos;         /**< position where reading stopped. Will be updated. Necessarily 0 <= pos <= size */
} InBuffer;

typedef struct OutBuffer_s {
    void*  dst;         /**< start of output buffer */
    size_t size;        /**< size of output buffer */
    size_t pos;         /**< position where writing stopped. Will be updated. Necessarily 0 <= pos <= size */
} OutBuffer;

typedef struct {
    /* Inherits from IByteOut */
    void (*Write)(void *p, Byte b);
    OutBuffer *outBuffer;
} BufferWriter;

typedef struct {
    /* Inherits from IByteIn */
    Byte (*Read)(void *p);
    InBuffer *inBuffer;
} BufferReader;

typedef struct ppmd8_args_s {
    CPpmd8 *cPpmd8;
    InBuffer *in;
    OutBuffer *out;
    int max_length;
    Bool finished;
    int result;
    pthread_t handle;
} ppmd8_args;

void Writer(const void *p, Byte b);
Byte Reader(const void *p);

Bool Ppmd8T_init(CPpmd8 *cPpmd8, UInt32 memory_size, unsigned int maximum_order, ISzAlloc *allocator);
int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd8_args *args);

#endif //PYPPMD_BUFFER_H

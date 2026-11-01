//
// Created by miurahr on 2021/08/06.
//

#ifndef PYPPMD_BUFFER_H
#define PYPPMD_BUFFER_H

#include "Ppmd7.h"
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

typedef struct ppmd_info_s {
    /* hold CPpmd8 or CPpmd7 struct pointer */
    void *cPpmd;
    void *rc;
    InBuffer *in;
    OutBuffer *out;
    int max_length;
    int result;
    void *t;
} ppmd_info;

typedef struct {
    /* Inherits from IByteOut */
    void (*Write)(void *p, Byte b);
    OutBuffer *outBuffer;
    ppmd_info *t;
    /* Called by Writer() when outBuffer is full. It must give outBuffer a new
       block (dst/size/pos updated in place) and return 0, or return non-zero
       on failure. NULL means the buffer cannot grow. */
    int (*Grow)(void *p);
    void *growArg;
    /* Set to non-zero when a byte could not be written. The caller must check
       it after each encoder call; the output is unusable once it is set. */
    int error;
} BufferWriter;

typedef struct {
    /* Inherits from IByteIn */
    Byte (*Read)(void *p);
    InBuffer *inBuffer;
    ppmd_info *t;
} BufferReader;


void Writer(const void *p, Byte b);
Byte Reader(const void *p);

#endif //PYPPMD_BUFFER_H

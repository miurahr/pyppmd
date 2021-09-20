//
// Created by miurahr on 2021/08/06.
//

#ifndef PYPPMD_BUFFER_H
#define PYPPMD_BUFFER_H

#include "Ppmd8.h"
#include "threading.h"

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

typedef struct ppmd_thread_info_s {
    /* hold CPpmd8 or CPpmd7 struct pointer */
    void *cPpmd;
    InBuffer *in;
    OutBuffer *out;
    int max_length;
    Bool endmark;
    Bool finished;
    int result;
    PPMD_pthread_t handle;
    PPMD_pthread_mutex_t mutex;
    PPMD_pthread_cond_t inEmpty;
    PPMD_pthread_cond_t notEmpty;
} ppmd_thread_info;

typedef struct {
    /* Inherits from IByteOut */
    void (*Write)(void *p, Byte b);
    OutBuffer *outBuffer;
    ppmd_thread_info *t;
} BufferWriter;

typedef struct {
    /* Inherits from IByteIn */
    Byte (*Read)(void *p);
    InBuffer *inBuffer;
    ppmd_thread_info *t;
} BufferReader;


void Writer(const void *p, Byte b);
Byte Reader(const void *p);

#endif //PYPPMD_BUFFER_H

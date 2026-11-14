//
// Created by miurahr on 2021/08/06.
//

#include "Buffer.h"

void Writer(const void *p, Byte b) {
    BufferWriter *bufferWriter = (BufferWriter *)p;
    OutBuffer *out = bufferWriter->outBuffer;
    if (out->pos == out->size) {
        /* The range encoder can emit several bytes for a single symbol, so the
           buffer has to grow here; dropping the byte corrupts the stream. */
        if (bufferWriter->error || bufferWriter->Grow == NULL
            || bufferWriter->Grow(bufferWriter) != 0 || out->pos == out->size) {
            bufferWriter->error = 1;
            return;
        }
    }
    *((Byte *)out->dst + out->pos++) = b;
}

Byte Reader(const void *p) {
    BufferReader *bufferReader = (BufferReader *)p;
    if (bufferReader->inBuffer->pos == bufferReader->inBuffer->size) {
        // TODO
    }
    return *((const Byte *)bufferReader->inBuffer->src + bufferReader->inBuffer->pos++);
}

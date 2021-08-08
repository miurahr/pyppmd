//
// Created by miurahr on 2021/08/06.
//

#include "Buffer.h"

void Writer(const void *p, Byte b) {
    BufferWriter *bufferWriter = (BufferWriter *)p;
    if (bufferWriter->outBuffer->size == bufferWriter->outBuffer->pos) {
        // FIXME: When out buffer is full
        return;
    }
    *((Byte *)bufferWriter->outBuffer->dst + bufferWriter->outBuffer->pos++) = b;
}

Byte Reader(const void *p) {
    BufferReader *bufferReader = (BufferReader *)p;
    if (bufferReader->inBuffer->pos == bufferReader->inBuffer->size) {
        // TODO
    }
    return *((const Byte *)bufferReader->inBuffer->src + bufferReader->inBuffer->pos++);
}

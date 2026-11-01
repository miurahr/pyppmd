//
// Synchronous (single threaded) Ppmd8 decoder.
//

#include "Ppmd8Decoder.h"

#define PPMD8_RESULT_EOF (-1)

static Byte Ppmd8Reader_Read(const IByteIn *p) {
    Ppmd8Reader *r = (Ppmd8Reader *) p;
    if (r->pos < r->size) {
        return r->src[r->pos++];
    }
    r->overrun++;
    return 0;
}

void Ppmd8Reader_Init(Ppmd8Reader *r) {
    r->vt.Read = Ppmd8Reader_Read;
    r->src = NULL;
    r->size = 0;
    r->pos = 0;
    r->overrun = 0;
}

void Ppmd8Reader_Set(Ppmd8Reader *r, const void *src, size_t size) {
    r->src = (const Byte *) src;
    r->size = size;
    r->pos = 0;
}

Ppmd8DecStatus Ppmd8_DecodeBlock(CPpmd8 *p, Ppmd8Reader *in, OutBuffer *out, size_t limit,
                                 Bool final, size_t *produced) {
    const size_t margin = PPMD8_MAX_BYTES_PER_SYMBOL(p->MaxOrder);
    Ppmd8DecStatus status;
    size_t n = 0;

    for (;;) {
        if (n >= limit) {
            status = PPMD8_DEC_LENGTH;
            break;
        }
        if (out->pos >= out->size) {
            status = PPMD8_DEC_OUT_FULL;
            break;
        }
        if (final) {
            if (in->overrun > margin) {
                status = PPMD8_DEC_EOF;
                break;
            }
        } else if (in->size - in->pos < margin) {
            status = PPMD8_DEC_NEED_INPUT;
            break;
        }
        const int c = Ppmd8_DecodeSymbol(p);
        if (!final && in->overrun > 0) {
            status = PPMD8_DEC_ERROR_OVERRUN;
            break;
        }
        if (c == PPMD8_RESULT_EOF) {
            status = PPMD8_DEC_EOF;
            break;
        }
        if (c < 0) {
            status = PPMD8_DEC_ERROR_DATA;
            break;
        }
        ((Byte *) out->dst)[out->pos++] = (Byte) c;
        n++;
    }
    *produced = n;
    return status;
}

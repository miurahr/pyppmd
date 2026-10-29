//
// Synchronous (single threaded) Ppmd7 decoder.
//

#include "Ppmd7Decoder.h"

#define PPMD7_RESULT_EOF (-1)

static Byte Ppmd7Reader_Read(const IByteIn *p) {
    Ppmd7Reader *r = (Ppmd7Reader *) p;
    if (r->pos < r->size) {
        return r->src[r->pos++];
    }
    r->overrun++;
    return 0;
}

void Ppmd7Reader_Init(Ppmd7Reader *r) {
    r->vt.Read = Ppmd7Reader_Read;
    r->src = NULL;
    r->size = 0;
    r->pos = 0;
    r->overrun = 0;
}

void Ppmd7Reader_Set(Ppmd7Reader *r, const void *src, size_t size) {
    r->src = (const Byte *) src;
    r->size = size;
    r->pos = 0;
}

Ppmd7DecStatus Ppmd7_DecodeBlock(CPpmd7 *p, CPpmd7z_RangeDec *rc, Ppmd7Reader *in,
                                 OutBuffer *out, size_t limit, Bool final, size_t *produced) {
    const size_t margin = PPMD7_MAX_BYTES_PER_SYMBOL(p->MaxOrder);
    Ppmd7DecStatus status;
    size_t n = 0;

    for (;;) {
        if (n >= limit) {
            status = PPMD7_DEC_LENGTH;
            break;
        }
        if (out->pos >= out->size) {
            status = PPMD7_DEC_OUT_FULL;
            break;
        }
        if (final) {
            if (in->overrun > margin) {
                status = PPMD7_DEC_EOF;
                break;
            }
        } else if (in->size - in->pos < margin) {
            status = PPMD7_DEC_NEED_INPUT;
            break;
        }
        const int c = Ppmd7_DecodeSymbol(p, rc);
        if (!final && in->overrun > 0) {
            status = PPMD7_DEC_ERROR_OVERRUN;
            break;
        }
        if (c == PPMD7_RESULT_EOF) {
            status = PPMD7_DEC_EOF;
            break;
        }
        if (c < 0) {
            status = PPMD7_DEC_ERROR_DATA;
            break;
        }
        ((Byte *) out->dst)[out->pos++] = (Byte) c;
        n++;
    }
    *produced = n;
    return status;
}

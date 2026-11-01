//
// Synchronous (single threaded) Ppmd8 decoder.
//

#ifndef PYPPMD_PPMD8DECODER_H
#define PYPPMD_PPMD8DECODER_H

#include "Buffer.h"

/*
 * The Ppmd8 range decoder (Subbotin's carryless coder) reads input only in the
 * normalization loop of RangeDec_Decode().  Every iteration multiplies Range by 256 and
 * needs Range < 2^24 to continue; when Range < kBot the loop resets Range to
 * (0 - Low) & (kBot - 1), which is >= 256^i after i iterations.  So one normalization reads
 * at most 3 bytes.  Ppmd8_DecodeSymbol() calls RangeDec_Decode() once for the first context
 * and once per escape step, and a suffix chain holds at most MaxOrder + 1 contexts, so a
 * symbol consumes at most 3 * (MaxOrder + 1) bytes.
 *
 * The decoder keeps 8 * (MaxOrder + 1) bytes (<= 136 for PPMD8_MAX_ORDER) as a safety margin.
 * A wrong bound can never read out of bounds: without `final`, running the reader dry is
 * reported as PPMD8_DEC_ERROR_OVERRUN instead of feeding zeros.
 */
#define PPMD8_MAX_BYTES_PER_SYMBOL(max_order) (8 * ((size_t)(max_order) + 1))

/* Ppmd8_RangeDec_Init() reads 4 bytes. */
#define PPMD8_RANGE_DEC_INIT_SIZE 4

/* Bounds-checked byte reader. Inherits from IByteIn (vt must be the first member). */
typedef struct {
    IByteIn vt;
    const Byte *src;
    size_t size;
    size_t pos;
    /* number of bytes requested after the end of input; they were returned as 0 */
    size_t overrun;
} Ppmd8Reader;

void Ppmd8Reader_Init(Ppmd8Reader *r);
void Ppmd8Reader_Set(Ppmd8Reader *r, const void *src, size_t size);

typedef enum {
    /* the requested number of symbols was produced */
    PPMD8_DEC_LENGTH = 0,
    /* the output buffer is full; grow it and call again */
    PPMD8_DEC_OUT_FULL,
    /* less than PPMD8_MAX_BYTES_PER_SYMBOL bytes of input are left and the input is not final */
    PPMD8_DEC_NEED_INPUT,
    /* end mark was decoded, or (final input only) the input is exhausted */
    PPMD8_DEC_EOF,
    /* corrupted data */
    PPMD8_DEC_ERROR_DATA,
    /* the reader was overrun while the input was not final */
    PPMD8_DEC_ERROR_OVERRUN
} Ppmd8DecStatus;

/*
 * Decode at most `limit` symbols into `out` (starting at out->pos, up to out->size).
 * `p->Stream.In` must point to `in`.  `*produced` receives the number of bytes written.
 * When `final` is false, a symbol is decoded only if at least PPMD8_MAX_BYTES_PER_SYMBOL
 * bytes of input are available, so the reader never runs dry.
 * When `final` is true, the reader returns zeros after the end of input, but decoding stops
 * with PPMD8_DEC_EOF once more than PPMD8_MAX_BYTES_PER_SYMBOL such bytes were requested.
 */
Ppmd8DecStatus Ppmd8_DecodeBlock(CPpmd8 *p, Ppmd8Reader *in, OutBuffer *out, size_t limit,
                                 Bool final, size_t *produced);

#endif //PYPPMD_PPMD8DECODER_H

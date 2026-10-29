//
// Synchronous (single threaded) Ppmd7 decoder.
//

#ifndef PYPPMD_PPMD7DECODER_H
#define PYPPMD_PPMD7DECODER_H

#include "Buffer.h"

/*
 * Ppmd7z_RangeDec reads input only in Range_Normalize(), and one call reads at most 2 bytes.
 * Ppmd7_DecodeSymbol() calls Range_Decode()/Range_DecodeBit() once for the first context and
 * once per escape step; a suffix chain holds at most MaxOrder + 1 contexts. Therefore a single
 * symbol consumes at most 2 * (MaxOrder + 1) bytes (<= 130 for PPMD7_MAX_ORDER).
 */
#define PPMD7_MAX_BYTES_PER_SYMBOL(max_order) (2 * ((size_t)(max_order) + 1))

/* Bounds-checked byte reader. Inherits from IByteIn (vt must be the first member). */
typedef struct {
    IByteIn vt;
    const Byte *src;
    size_t size;
    size_t pos;
    /* number of bytes requested after the end of input; they were returned as 0 */
    size_t overrun;
} Ppmd7Reader;

void Ppmd7Reader_Init(Ppmd7Reader *r);
void Ppmd7Reader_Set(Ppmd7Reader *r, const void *src, size_t size);

typedef enum {
    /* the requested number of symbols was produced */
    PPMD7_DEC_LENGTH = 0,
    /* the output buffer is full; grow it and call again */
    PPMD7_DEC_OUT_FULL,
    /* less than PPMD7_MAX_BYTES_PER_SYMBOL bytes of input are left and the input is not final */
    PPMD7_DEC_NEED_INPUT,
    /* end mark was decoded, or (final input only) the input is exhausted */
    PPMD7_DEC_EOF,
    /* corrupted data */
    PPMD7_DEC_ERROR_DATA,
    /* the reader was overrun while the input was not final: internal bug */
    PPMD7_DEC_ERROR_OVERRUN
} Ppmd7DecStatus;

/*
 * Decode at most `limit` symbols into `out` (starting at out->pos, up to out->size).
 * `*produced` receives the number of bytes written.
 * When `final` is false, a symbol is decoded only if at least PPMD7_MAX_BYTES_PER_SYMBOL
 * bytes of input are available, so the reader never runs dry.
 * When `final` is true, the reader returns zeros after the end of input, but decoding stops
 * with PPMD7_DEC_EOF once more than PPMD7_MAX_BYTES_PER_SYMBOL such bytes were requested.
 */
Ppmd7DecStatus Ppmd7_DecodeBlock(CPpmd7 *p, CPpmd7z_RangeDec *rc, Ppmd7Reader *in,
                                 OutBuffer *out, size_t limit, Bool final, size_t *produced);

#endif //PYPPMD_PPMD7DECODER_H

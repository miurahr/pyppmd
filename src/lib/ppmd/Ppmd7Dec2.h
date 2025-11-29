/* Ppmd7Dec2.h -- PPMdH Decoder (push / buffer-driven)
   2025-11-29 : pyppmd project : LGPL2+

   This API provides a push-style range decoder that consumes caller-supplied
   input buffers and produces decoded bytes into a caller-supplied output
   buffer. When more input is required to continue, it reports NEED_INPUT and
   the caller can resume by providing more input with the next call.

   It reuses the PPMd7 model (CPpmd7) and a updated logic from Ppmd7.h/.c.
*/

#ifndef PPMD7DEC2_H
#define PPMD7DEC2_H

#include "Ppmd7.h"

typedef struct
{
  UInt32 Range;
  UInt32 Code;
  /* input buffer */
  const Byte *in;
  size_t in_size;
  size_t in_pos;
  /* initialization state: header (first 5 bytes) consumed */
  int initialized;
  size_t init_count; /* number of header bytes consumed [0..5] */
} CPpmd7t_RangeDec;

typedef enum {
  PPMD7T_STATUS_OK = 0,        /* decoded some output, can continue */
  PPMD7T_STATUS_NEED_INPUT = 1,/* need more input to proceed */
  PPMD7T_STATUS_END = 2,       /* stream finished OK (RangeDec_IsFinishedOK==true) */
  PPMD7T_STATUS_ERROR = -1     /* error (data or internal) */
} Ppmd7tStatus;

/* Reset internal state. Call before first use or to restart new stream. */
void Ppmd7t_RangeDec_Reset(CPpmd7t_RangeDec *rc);

/* Set/append input buffer for the decoder to consume. The buffer is not copied;
   the caller must ensure it remains valid during the call. */
void Ppmd7t_RangeDec_SetInput(CPpmd7t_RangeDec *rc, const Byte *data, size_t size);

/* Decode into out buffer up to out_cap bytes. Consumes from the current input
   buffer (set by SetInput) and reports how much input was consumed.
   On return:
     - *out_written = number of bytes produced into out
     - *in_consumed = number of input bytes consumed from the provided buffer
     - returns one of Ppmd7tStatus values
   The function can be called repeatedly. When NEED_INPUT is returned, provide
   more input via SetInput() and call again. */
Ppmd7tStatus Ppmd7t_Decode(CPpmd7 *model,
                           CPpmd7t_RangeDec *rc,
                           Byte *out, size_t out_cap,
                           size_t *out_written,
                           size_t *in_consumed,
                           int *finished_ok);

#endif /* PPMD7DEC2_H */

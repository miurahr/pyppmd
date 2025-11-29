/* Ppmd7Dec2.c -- PPMdH Decoder (push / buffer-driven)
   2025-11-29 : pyppmd project : Public domain

   This file provides a push-based decoding API that consumes caller-supplied
   buffers and can pause with NEED_INPUT when more data is required.
*/

#include "Ppmd7Dec2.h"

#define kTopValue (1u << 24)

static int rc_need_byte(CPpmd7t_RangeDec *p, UInt32 *outByte)
{
  if (p->in_pos >= p->in_size)
    return 0;
  *outByte = p->in[p->in_pos++];
  return 1;
}

void Ppmd7t_RangeDec_Reset(CPpmd7t_RangeDec *p)
{
  p->Range = 0xFFFFFFFFu;
  p->Code = 0;
  p->in = NULL;
  p->in_size = 0;
  p->in_pos = 0;
  p->initialized = 0;
  p->init_count = 0;
}

void Ppmd7t_RangeDec_SetInput(CPpmd7t_RangeDec *p, const Byte *data, size_t size)
{
  p->in = data;
  p->in_size = size;
  p->in_pos = 0;
}

static int Range_Init_Progress(CPpmd7t_RangeDec *p)
{
  /* We need first 5 bytes (one leading zero, then 4 for Code). */
  while (p->init_count < 5) {
    UInt32 b;
    if (!rc_need_byte(p, &b))
      return 0; /* need more input */
    if (p->init_count == 0) {
      /* first byte must be 0 */
      if (b != 0)
        return -1; /* error */
    } else {
      p->Code = (p->Code << 8) | b;
    }
    p->init_count++;
  }
  p->initialized = 1;
  return 1;
}

static inline int Range_Normalize_Push(CPpmd7t_RangeDec *p)
{
  if (p->Range < kTopValue) {
    UInt32 b;
    if (!rc_need_byte(p, &b))
      return 0; /* need more input */
    p->Code = (p->Code << 8) | b;
    p->Range <<= 8;
    if (p->Range < kTopValue) {
      if (!rc_need_byte(p, &b))
        return 0; /* need more input */
      p->Code = (p->Code << 8) | b;
      p->Range <<= 8;
    }
  }
  return 1;
}

static inline UInt32 Range_GetThreshold_Push(CPpmd7t_RangeDec *p, UInt32 total)
{
  return p->Code / (p->Range /= total);
}

static inline int Range_Decode_Push(CPpmd7t_RangeDec *p, UInt32 start, UInt32 size)
{
  p->Code -= start * p->Range;
  p->Range *= size;
  return Range_Normalize_Push(p);
}

static inline int Range_DecodeBit_Push(CPpmd7t_RangeDec *p, UInt32 size0, UInt32 *symbol)
{
  UInt32 newBound = (p->Range >> 14) * size0;
  if (p->Code < newBound) {
    *symbol = 0;
    p->Range = newBound;
  } else {
    *symbol = 1;
    p->Code -= newBound;
    p->Range -= newBound;
  }
  return Range_Normalize_Push(p);
}

#define MASK(sym) ((signed char *)charMask)[sym]

static int Ppmd7t_DecodeSymbol_Push(CPpmd7 *p, CPpmd7t_RangeDec *rc)
{
  size_t charMask[256 / sizeof(size_t)];

  if (p->MinContext->NumStats != 1) {
    CPpmd_State *s = Ppmd7_GetStats(p, p->MinContext);
    unsigned i;
    UInt32 count, hiCnt;
    if ((count = Range_GetThreshold_Push(rc, p->MinContext->SummFreq)) < (hiCnt = s->Freq)) {
      Byte symbol;
      if (!Range_Decode_Push(rc, 0, s->Freq)) return -3; /* need more input */
      p->FoundState = s;
      symbol = s->Symbol;
      Ppmd7_Update1_0(p);
      return symbol;
    }
    p->PrevSuccess = 0;
    i = p->MinContext->NumStats - 1;
    do {
      if ((hiCnt += (++s)->Freq) > count) {
        Byte symbol;
        if (!Range_Decode_Push(rc, hiCnt - s->Freq, s->Freq)) return -3;
        p->FoundState = s;
        symbol = s->Symbol;
        Ppmd7_Update1(p);
        return symbol;
      }
    } while (--i);
    if (count >= p->MinContext->SummFreq)
      return -2;
    p->HiBitsFlag = p->HB2Flag[p->FoundState->Symbol];
    if (!Range_Decode_Push(rc, hiCnt, p->MinContext->SummFreq - hiCnt)) return -3;
    PPMD_SetAllBitsIn256Bytes(charMask);
    MASK(s->Symbol) = 0;
    i = p->MinContext->NumStats - 1;
    do { MASK((--s)->Symbol) = 0; } while (--i);
  } else {
    UInt16 *prob = Ppmd7_GetBinSumm(p);
    UInt32 bit;
    if (!Range_DecodeBit_Push(rc, *prob, &bit)) return -3;
    if (bit == 0) {
      Byte symbol;
      *prob = (UInt16)PPMD_UPDATE_PROB_0(*prob);
      symbol = (p->FoundState = Ppmd7Context_OneState(p->MinContext))->Symbol;
      Ppmd7_UpdateBin(p);
      return symbol;
    }
    *prob = (UInt16)PPMD_UPDATE_PROB_1(*prob);
    p->InitEsc = PPMD7_kExpEscape[*prob >> 10];
    PPMD_SetAllBitsIn256Bytes(charMask);
    MASK(Ppmd7Context_OneState(p->MinContext)->Symbol) = 0;
    p->PrevSuccess = 0;
  }

  for (;;) {
    CPpmd_State *ps[256], *s;
    UInt32 freqSum, count, hiCnt;
    CPpmd_See *see;
    unsigned i, num, numMasked = p->MinContext->NumStats;
    do {
      p->OrderFall++;
      if (!p->MinContext->Suffix)
        return -1;
      p->MinContext = Ppmd7_GetContext(p, p->MinContext->Suffix);
    } while (p->MinContext->NumStats == numMasked);
    hiCnt = 0;
    s = Ppmd7_GetStats(p, p->MinContext);
    i = 0;
    num = p->MinContext->NumStats - numMasked;
    do {
      int k = (int)(MASK(s->Symbol));
      hiCnt += (s->Freq & k);
      ps[i] = s++;
      i -= k;
    } while (i != num);

    see = Ppmd7_MakeEscFreq(p, numMasked, &freqSum);
    freqSum += hiCnt;
    count = Range_GetThreshold_Push(rc, freqSum);

    if (count < hiCnt) {
      Byte symbol;
      CPpmd_State **pps = ps;
      for (hiCnt = 0; (hiCnt += (*pps)->Freq) <= count; pps++);
      s = *pps;
      if (!Range_Decode_Push(rc, hiCnt - s->Freq, s->Freq)) return -3;
      Ppmd_See_Update(see);
      p->FoundState = s;
      symbol = s->Symbol;
      Ppmd7_Update2(p);
      return symbol;
    }
    if (count >= freqSum)
      return -2;
    if (!Range_Decode_Push(rc, hiCnt, freqSum - hiCnt)) return -3;
    see->Summ = (UInt16)(see->Summ + freqSum);
    do { MASK(ps[--i]->Symbol) = 0; } while (i != 0);
  }
}

Ppmd7tStatus Ppmd7t_Decode(CPpmd7 *model,
                           CPpmd7t_RangeDec *rc,
                           Byte *out, size_t out_cap,
                           size_t *out_written,
                           size_t *in_consumed,
                           int *finished_ok)
{
  size_t produced = 0;

  if (out_written) *out_written = 0;
  if (in_consumed) *in_consumed = 0;
  if (finished_ok) *finished_ok = 0;

  if (!rc->initialized) {
    int r = Range_Init_Progress(rc);
    if (r == 0) {
      if (in_consumed) *in_consumed = rc->in_pos;
      return PPMD7T_STATUS_NEED_INPUT;
    }
    if (r < 0)
      return PPMD7T_STATUS_ERROR;
  }

  for (;;) {
    if (produced >= out_cap)
      break;
    {
      int sym = Ppmd7t_DecodeSymbol_Push(model, rc);
      if (sym >= 0) {
        out[produced++] = (Byte)sym;
        continue;
      }
      if (sym == -3) {
        /* need more input */
        if (out_written) *out_written = produced;
        if (in_consumed) *in_consumed = rc->in_pos;
        return produced ? PPMD7T_STATUS_OK : PPMD7T_STATUS_NEED_INPUT;
      }
      if (sym == -1) {
        /* EOCP: check finish */
        if (rc->Code == 0) {
          if (finished_ok) *finished_ok = 1;
          if (out_written) *out_written = produced;
          if (in_consumed) *in_consumed = rc->in_pos;
          return PPMD7T_STATUS_END;
        }
        return PPMD7T_STATUS_ERROR;
      }
      /* sym == -2 : data error */
      return PPMD7T_STATUS_ERROR;
    }
  }

  if (out_written) *out_written = produced;
  if (in_consumed) *in_consumed = rc->in_pos;
  return produced ? PPMD7T_STATUS_OK : PPMD7T_STATUS_OK;
}

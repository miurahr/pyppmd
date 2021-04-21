import sys

import cffi  # type: ignore  # noqa


def is_64bit() -> bool:
    return sys.maxsize > 2 ** 32


ffibuilder = cffi.FFI()

# ----------- PPMd interfaces ---------------------
# Interface.h
ffibuilder.cdef(
    r"""
typedef unsigned char Byte;
typedef short Int16;
typedef unsigned short UInt16;
typedef int Int32;
typedef unsigned int UInt32;
typedef long long Int64;
typedef unsigned long long UInt64;
typedef int Bool;
typedef struct IByteIn IByteIn;
struct IByteIn
{
  Byte (*Read)(const IByteIn *p); /* reads one byte, returns 0 in case of EOF or error */
};
typedef struct IByteOut IByteOut;
struct IByteOut
{
  void (*Write)(const IByteOut *p, Byte b);
};
struct ISzAlloc
{
  void *(*Alloc)(size_t size);
  void (*Free)(void *address); /* address can be NULL */
};
typedef struct ISzAlloc ISzAlloc;

typedef struct PPMD_inBuffer_s {
    const void* src;    /**< start of input buffer */
    size_t size;        /**< size of input buffer */
    size_t pos;         /**< position where reading stopped. Will be updated. Necessarily 0 <= pos <= size */
} PPMD_inBuffer;

typedef struct PPMD_outBuffer_s {
    void*  dst;         /**< start of output buffer */
    size_t size;        /**< size of output buffer */
    size_t pos;         /**< position where writing stopped. Will be updated. Necessarily 0 <= pos <= size */
} PPMD_outBuffer;
"""
)

# Ppmd.h
# Ppmd7.h
ffibuilder.cdef(
    r"""
/* SEE-contexts for PPM-contexts with masked symbols */
typedef struct
{
  UInt16 Summ;
  Byte Shift;
  Byte Count;
  ...;
} CPpmd_See;
typedef struct
{
  Byte Symbol;
  Byte Freq;
  UInt16 SuccessorLow;
  UInt16 SuccessorHigh;
} CPpmd_State;
"""
)

if is_64bit():
    ffibuilder.cdef("typedef UInt32 CPpmd_State_Ref;")
    ffibuilder.cdef("typedef UInt32 CPpmd_Void_Ref;")
    ffibuilder.cdef("typedef UInt32 CPpmd7_Context_Ref;")
else:
    ffibuilder.cdef("typedef CPpmd_State * CPpmd_State_Ref;")
    ffibuilder.cdef("typedef void * CPpmd_Void_Ref;")
    ffibuilder.cdef("struct CPpmd7_Context_; typedef struct CPpmd7_Context_ * CPpmd7_Context_Ref;")

ffibuilder.cdef(
    r"""
typedef struct CPpmd7_Context_
{
  UInt16 NumStats;
  UInt16 SummFreq;
  CPpmd_State_Ref Stats;
  CPpmd7_Context_Ref Suffix;
} CPpmd7_Context;

typedef struct
{
  CPpmd7_Context *MinContext, *MaxContext;
  CPpmd_State *FoundState;
  unsigned OrderFall, InitEsc, PrevSuccess, MaxOrder, HiBitsFlag;
  Int32 RunLength, InitRL;

  UInt32 Size;
  UInt32 GlueCount;
  Byte *Base, *LoUnit, *HiUnit, *Text, *UnitsStart;
  UInt32 AlignOffset;

  Byte Indx2Units[38];
  Byte Units2Indx[128];
  CPpmd_Void_Ref FreeList[38];
  Byte NS2Indx[256], NS2BSIndx[256], HB2Flag[256];
  CPpmd_See DummySee, See[25][16];
  UInt16 BinSumm[128][64];
} CPpmd7;
typedef struct
{
  UInt32 Range;
  UInt32 Code;
  IByteIn *Stream;
} CPpmd7z_RangeDec;
typedef struct
{
  UInt64 Low;
  UInt32 Range;
  Byte Cache;
  UInt64 CacheSize;
  IByteOut *Stream;
} CPpmd7z_RangeEnc;
"""
)

# Ppmd8.h
if is_64bit():
    ffibuilder.cdef("typedef UInt32 CPpmd8_Context_Ref;")
else:
    ffibuilder.cdef("typedef struct CPpmd8_Context_ * CPpmd8_Context_Ref;")

ffibuilder.cdef(
    r"""
typedef struct CPpmd8_Context_
{
  Byte NumStats;
  Byte Flags;
  UInt16 SummFreq;
  CPpmd_State_Ref Stats;
  CPpmd8_Context_Ref Suffix;
} CPpmd8_Context;

typedef struct
{
  CPpmd8_Context *MinContext, *MaxContext;
  CPpmd_State *FoundState;
  unsigned OrderFall, InitEsc, PrevSuccess, MaxOrder;
  Int32 RunLength, InitRL; /* must be 32-bit at least */

  UInt32 Size;
  UInt32 GlueCount;
  Byte *Base, *LoUnit, *HiUnit, *Text, *UnitsStart;
  UInt32 AlignOffset;
  unsigned RestoreMethod;

  /* Range Coder */
  UInt32 Range;
  UInt32 Code;
  UInt32 Low;
  union
  {
    IByteIn *In;
    IByteOut *Out;
  } Stream;

  Byte Indx2Units[38];
  Byte Units2Indx[128];
  CPpmd_Void_Ref FreeList[38];
  UInt32 Stamps[38];

  Byte NS2BSIndx[256], NS2Indx[260];
  CPpmd_See DummySee, See[24][32];
  UInt16 BinSumm[25][64];
} CPpmd8;
"""
)

# ----------- python binding API ---------------------
ffibuilder.cdef(
    r"""
extern "Python" void *raw_alloc(size_t);
extern "Python" void raw_free(void *);

typedef struct {
    /* Inherits from IByteOut */
    void (*Write)(void *p, Byte b);
    PPMD_outBuffer *outBuffer;
} BufferWriter;

typedef struct {
    /* Inherits from IByteIn */
    Byte (*Read)(void *p);
    PPMD_inBuffer *inBuffer;
} BufferReader;

void ppmd7_state_init(CPpmd7 *ppmd, unsigned int maxOrder, unsigned int memSize, ISzAlloc *allocator);
void ppmd7_state_close(CPpmd7 *ppmd, ISzAlloc *allocator);
int ppmd7_decompress_init(CPpmd7z_RangeDec *rc, BufferReader *reader);
void ppmd7_compress_init(CPpmd7z_RangeEnc *rc, BufferWriter *write);

int ppmd7_compress(CPpmd7 *p, CPpmd7z_RangeEnc *rc, PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf);
void ppmd7_compress_flush(CPpmd7z_RangeEnc *rc);
int ppmd7_decompress(CPpmd7 *p, CPpmd7z_RangeDec *rc,PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf, size_t length);
void ppmd7_decompress_flush(CPpmd7 *p, CPpmd7z_RangeDec *rc,PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf, size_t length);

void Ppmd7_Construct(CPpmd7 *p);
void Ppmd7_Init(CPpmd7 *p, unsigned maxOrder);
int Ppmd7_DecodeSymbol(CPpmd7 *p, CPpmd7z_RangeDec *rc);

void Ppmd7z_RangeEnc_Init(CPpmd7z_RangeEnc *p);
void Ppmd7z_RangeEnc_FlushData(CPpmd7z_RangeEnc *p);
void Ppmd7_EncodeSymbol(CPpmd7 *p, CPpmd7z_RangeEnc *rc, int symbol);

void ppmd8_compress_init(CPpmd8 *ppmd, BufferWriter *writer);
int ppmd8_compress(CPpmd8 *ppmd, PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf);
void ppmd8_decompress_init(CPpmd8 *ppmd, BufferReader *reader);
void ppmd8_decompress(CPpmd8 *p, PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf, size_t length);
void ppmd8_decompress_flush(CPpmd8 *p, PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf, size_t length);

void Ppmd8_Construct(CPpmd8 *ppmd);
Bool Ppmd8_Alloc(CPpmd8 *p, UInt32 size, ISzAlloc *alloc);
void Ppmd8_Free(CPpmd8 *p, ISzAlloc *alloc);
void Ppmd8_Init(CPpmd8 *ppmd, unsigned maxOrder, unsigned restoreMethod);
void Ppmd8_EncodeSymbol(CPpmd8 *ppmd, int symbol);
void Ppmd8_RangeEnc_Init(CPpmd8 *ppmd);
void Ppmd8_RangeEnc_FlushData(CPpmd8 *ppmd);
Bool Ppmd8_RangeDec_Init(CPpmd8 *ppmd);
int Ppmd8_DecodeSymbol(CPpmd8 *ppmd);
"""
)

source = r"""
#include "Ppmd7.h"
#include "Ppmd8.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define getc_unlocked fgetc
#define putc_unlocked fputc
#endif

typedef struct {
    /* Inherits from IByteOut */
    void (*Write)(void *p, Byte b);
    PPMD_outBuffer *outBuffer;
} BufferWriter;

typedef struct {
    /* Inherits from IByteIn */
    Byte (*Read)(void *p);
    PPMD_inBuffer *inBuffer;
} BufferReader;

static void Write(void *p, Byte b)
{
    BufferWriter *bw = p;
    PPMD_outBuffer *buf = bw->outBuffer;
    if (buf->pos < buf->size) {
        *((Byte *)buf->dst + buf->pos++) = b;
    }
}

static Byte Read(void *p)
{
    BufferReader *br = p;
    PPMD_inBuffer *inBuffer = br->inBuffer;
    if (inBuffer->pos == inBuffer->size) {
        return -1;
    } else {
        return *((const Byte *)inBuffer->src + inBuffer->pos++);
    }
}

void ppmd7_state_init(CPpmd7 *p, unsigned int maxOrder, unsigned int memSize, ISzAlloc *allocator)
{
    Ppmd7_Construct(p);
    Ppmd7_Alloc(p, memSize, allocator);
    Ppmd7_Init(p, maxOrder);
}

void ppmd7_state_close(CPpmd7 *ppmd, ISzAlloc *allocator)
{
    Ppmd7_Free(ppmd, allocator);
}

void ppmd7_compress_init(CPpmd7z_RangeEnc *rc, BufferWriter *writer)
{
    writer->Write = Write;
    rc->Stream = (IByteOut *) writer;
    Ppmd7z_RangeEnc_Init(rc);
}

int ppmd7_decompress_init(CPpmd7z_RangeDec *rc, BufferReader *reader)
{
    reader->Read = Read;
    rc->Stream = (IByteIn *) reader;
    Bool res = Ppmd7z_RangeDec_Init(rc);
    return res;
}

int ppmd7_compress(CPpmd7 *p, CPpmd7z_RangeEnc *rc, PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf) {
    Byte* c = (Byte *) in_buf->src + in_buf->pos;
    const Byte* in_end = (Byte *)in_buf->src + in_buf->size;
    while (c < in_end) {
        Ppmd7_EncodeSymbol(p, rc, *c++);
        if (out_buf->pos >= out_buf->size) {
            break;
        }
    }
    in_buf->pos = c - (Byte *)in_buf->src;
    return in_buf->size - in_buf->pos;
}

void ppmd7_compress_flush(CPpmd7z_RangeEnc *rc){
    Ppmd7z_RangeEnc_FlushData(rc);
}

int ppmd7_decompress(CPpmd7 *p, CPpmd7z_RangeDec *rc,PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf, size_t length) {
    Byte* c = (Byte *) out_buf->dst + out_buf->pos;
    const size_t out_start = out_buf->pos;
    const Byte* out_end = (Byte *)out_buf->dst + length;
    while (c < out_end) {
        *c++ = Ppmd7_DecodeSymbol(p, rc);
        if (in_buf->pos == in_buf->size) {
            break;
        }
    }
    out_buf->pos = c - (Byte *)out_buf->dst;
    return out_buf->pos - out_start;
}

void ppmd7_decompress_flush(CPpmd7 *p, CPpmd7z_RangeDec *rc,PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf, size_t length) {
    Byte* c = (Byte *) out_buf->dst + out_buf->pos;
    const Byte* out_end = (Byte *)out_buf->dst + length;
    while (c < out_end) {
        *c++ = Ppmd7_DecodeSymbol(p, rc);
    }
    out_buf->pos = c - (Byte *)out_buf->dst;
}

void ppmd8_compress_init(CPpmd8 *ppmd, BufferWriter *writer)
{
    writer->Write = Write;
    ppmd->Stream.Out = (IByteOut *) writer;
}

int ppmd8_compress(CPpmd8 *ppmd, PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf) {
    Byte* c = (Byte *) in_buf->src + in_buf->pos;
    const Byte* in_end = (Byte *)in_buf->src + in_buf->size;
    while (c < in_end) {
        Ppmd8_EncodeSymbol(ppmd, *c++);
        if (out_buf->pos >= out_buf->size) {
            break;
        }
    }
    in_buf->pos = c - (Byte *)in_buf->src;
    return in_buf->size - in_buf->pos;
}

void ppmd8_decompress_init(CPpmd8 *ppmd,  BufferReader *reader)
{
    reader->Read = Read;
    ppmd->Stream.In = (IByteIn *) reader;
}

void ppmd8_decompress(CPpmd8 *ppmd, PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf, size_t length) {
    Byte* c = (Byte *) out_buf->dst + out_buf->pos;
    const Byte* out_end = (Byte *)out_buf->dst + length;
    while (c < out_end) {
        *c++ = Ppmd8_DecodeSymbol(ppmd);
        if (in_buf->pos == in_buf->size) {
            break;
        }
    }
    out_buf->pos = c - (Byte *)out_buf->dst;
}

void ppmd8_decompress_flush(CPpmd8 *ppmd, PPMD_outBuffer *out_buf, PPMD_inBuffer *in_buf, size_t length) {
    Byte* c = (Byte *) out_buf->dst + out_buf->pos;
    const Byte* out_end = (Byte *)out_buf->dst + length;
    while (c < out_end) {
        *c++ = Ppmd8_DecodeSymbol(ppmd);
    }
    out_buf->pos = c - (Byte *)out_buf->dst;
}
"""


def set_kwargs(**kwargs):
    ffibuilder.set_source(source=source, **kwargs)


if __name__ == "__main__":  # not when running with setuptools
    ffibuilder.compile(verbose=True)

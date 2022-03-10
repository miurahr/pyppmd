import sys

import cffi  # type: ignore  # noqa


def is_64bit() -> bool:
    return sys.maxsize > 1 << 32


ffibuilder = cffi.FFI()

# ----------- PPMd interfaces ---------------------
# Interface.h
# Buffer.h
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
struct IAlloc
{
  void *(*Alloc)(size_t size);
  void (*Free)(void *address); /* address can be NULL */
};
typedef struct IAlloc IAlloc;
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

if sys.platform.startswith("win32"):
    ffibuilder.cdef(
        r"""
typedef struct _pthread_cleanup _pthread_cleanup;
struct _pthread_cleanup
{
    void (*func)(void *);
    void *arg;
    _pthread_cleanup *next;
};
struct _pthread_v
{
    void *ret_arg;
    void *(* func)(void *);
    _pthread_cleanup *clean;
    HANDLE h;
    int cancelled;
    unsigned p_state;
    int keymax;
    void **keyval;
    // hide it
    // jmp_buf jb;
};
typedef struct _pthread_v *pthread_t;
    """
    )
elif sys.platform.startswith("darwin"):
    ffibuilder.cdef(r"typedef void* pthread_t;")
elif sys.platform.startswith("linux"):
    ffibuilder.cdef(r"typedef unsigned long int pthread_t;")
else:
    pass  # todo

ffibuilder.cdef(
    r"""
typedef struct ppmd_info_s {
    void *cPpmd;
    void *rc;
    InBuffer *in;
    OutBuffer *out;
    int max_length;
    int result;
    void *t;
} ppmd_info;

Byte Ppmd_thread_Reader(const void *p);
int Ppmd7T_decode(CPpmd7 *cPpmd7, CPpmd7z_RangeDec *rc, OutBuffer *out, int max_length, ppmd_info *args);
void Ppmd7T_Free(CPpmd7 *cPpmd7, ppmd_info *args, IAlloc *allocator);
int Ppmd8T_decode(CPpmd8 *cPpmd8, OutBuffer *out, int max_length, ppmd_info *args);
void Ppmd8T_Free(CPpmd8 *cPpmd8, ppmd_info *args, IAlloc *allocator);
"""
)

# ----------- python binding API ---------------------
ffibuilder.cdef(
    r"""
extern "Python" void *raw_alloc(size_t);
extern "Python" void raw_free(void *);

void Writer(const void *p, Byte b);
Byte Reader(const void *p);

typedef struct {
    /* Inherits from IByteOut */
    void (*Write)(void *p, Byte b);
    OutBuffer *outBuffer;
    ppmd_info *t;
} BufferWriter;

typedef struct {
    /* Inherits from IByteIn */
    Byte (*Read)(void *p);
    InBuffer *inBuffer;
    ppmd_info *t;
} BufferReader;

void ppmd7_state_init(CPpmd7 *ppmd, unsigned int maxOrder, unsigned int memSize, IAlloc *allocator);
void ppmd7_state_close(CPpmd7 *ppmd, IAlloc *allocator);

void ppmd7_compress_init(CPpmd7z_RangeEnc *rc, BufferWriter *write);
int ppmd7_decompress_init(CPpmd7z_RangeDec *rc, BufferReader *reader, ppmd_info *info, IAlloc *allocator);

int ppmd7_compress(CPpmd7 *p, CPpmd7z_RangeEnc *rc, OutBuffer *out_buf, InBuffer *in_buf);
void ppmd7_compress_flush(CPpmd7 *p, CPpmd7z_RangeEnc *rc, Bool endmark);
int ppmd7_decompress(CPpmd7 *p, CPpmd7z_RangeDec *rc, OutBuffer *out_buf, InBuffer *in_buf, size_t length, ppmd_info *info);

void Ppmd7_Construct(CPpmd7 *p);
void Ppmd7_Init(CPpmd7 *p, unsigned maxOrder);
int Ppmd7_DecodeSymbol(CPpmd7 *p, CPpmd7z_RangeDec *rc);

void Ppmd7z_RangeEnc_Init(CPpmd7z_RangeEnc *p);
void Ppmd7z_RangeEnc_FlushData(CPpmd7z_RangeEnc *p);
void Ppmd7_EncodeSymbol(CPpmd7 *p, CPpmd7z_RangeEnc *rc, int symbol);

void ppmd8_compress_init(CPpmd8 *ppmd, BufferWriter *writer);
int ppmd8_compress(CPpmd8 *ppmd, OutBuffer *out_buf, InBuffer *in_buf);
void ppmd8_decompress_init(CPpmd8 *ppmd, BufferReader *reader, ppmd_info *threadInfo, IAlloc *allocator);
int ppmd8_decompress(CPpmd8 *ppmd, OutBuffer *out_buf, InBuffer *in_buf, int length, ppmd_info *threadInfo);

void Ppmd8_Construct(CPpmd8 *ppmd);
Bool Ppmd8_Alloc(CPpmd8 *p, UInt32 size, IAlloc *alloc);
void Ppmd8_Free(CPpmd8 *p, IAlloc *alloc);
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
#include "Buffer.h"
#include "ThreadDecoder.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include "win_pthreads.h"
#define getc_unlocked fgetc
#define putc_unlocked fputc
#else
#include <pthread.h>
#endif

void ppmd7_state_init(CPpmd7 *p, unsigned int maxOrder, unsigned int memSize, IAlloc *allocator)
{
    Ppmd7_Construct(p);
    Ppmd7_Alloc(p, memSize, allocator);
    Ppmd7_Init(p, maxOrder);
}

void ppmd7_state_close(CPpmd7 *ppmd, IAlloc *allocator)
{
    Ppmd7_Free(ppmd, allocator);
}

void ppmd7_compress_init(CPpmd7z_RangeEnc *rc, BufferWriter *writer)
{
    writer->Write = (void (*)(void *, Byte)) Writer;
    rc->Stream = (IByteOut *) writer;
    Ppmd7z_RangeEnc_Init(rc);
}

int ppmd7_decompress_init(CPpmd7z_RangeDec *rc, BufferReader *reader, ppmd_info *info, IAllocPtr allocator)
{
    reader->Read = (Byte (*)(void *)) Reader;
    rc->Stream = (IByteIn *) reader;
    Bool res = Ppmd7z_RangeDec_Init(rc);
    Ppmd_thread_decode_init(info, allocator);
    return res;
}

int ppmd7_compress(CPpmd7 *p, CPpmd7z_RangeEnc *rc, OutBuffer *out_buf, InBuffer *in_buf) {
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

void ppmd7_compress_flush(CPpmd7 *p, CPpmd7z_RangeEnc *rc, Bool endmark){
    if (endmark) {
        Ppmd7_EncodeSymbol(p, rc, -1);
    }
    Ppmd7z_RangeEnc_FlushData(rc);
}

int ppmd7_decompress(CPpmd7 *ppmd, CPpmd7z_RangeDec *rc, OutBuffer *out_buf, InBuffer *in_buf, int length, ppmd_info *info) {
    info->in = in_buf;
    info->out = out_buf;
    return Ppmd7T_decode(ppmd, rc, out_buf, length, info);
}

void ppmd8_compress_init(CPpmd8 *ppmd, BufferWriter *writer)
{
    writer->Write = (void (*)(void *, Byte)) Writer;
    ppmd->Stream.Out = (IByteOut *) writer;
}

int ppmd8_compress(CPpmd8 *ppmd, OutBuffer *out_buf, InBuffer *in_buf) {
    Byte* pos = (Byte *) in_buf->src + in_buf->pos;
    const Byte* in_end = (Byte *)in_buf->src + in_buf->size;
    while (pos < in_end) {
        Byte c = *pos++;
        Ppmd8_EncodeSymbol(ppmd, c);
        if (out_buf->pos >= out_buf->size) {
            break;
        }
    }
    in_buf->pos = pos - (Byte *)in_buf->src;
    return in_buf->size - in_buf->pos;
}

void ppmd8_decompress_init(CPpmd8 *ppmd, BufferReader *reader, ppmd_info *info, IAllocPtr allocator)
{
    reader->Read = (Byte (*)(void *)) Ppmd_thread_Reader;
    ppmd->Stream.In = (IByteIn *) reader;
    reader->t = info;
    info->in = reader->inBuffer;
    Ppmd_thread_decode_init(info, allocator);
}

int ppmd8_decompress(CPpmd8 *ppmd, OutBuffer *out_buf, InBuffer *in_buf, int length, ppmd_info *info) {
    info->in = in_buf;
    return Ppmd8T_decode(ppmd, out_buf, length, info);
}
"""


def set_kwargs(**kwargs):
    ffibuilder.set_source(source=source, **kwargs)


if __name__ == "__main__":  # not when running with setuptools
    ffibuilder.compile(verbose=True)

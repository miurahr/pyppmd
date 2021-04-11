import sys

import cffi  # type: ignore  # noqa


def is_64bit() -> bool:
    return sys.maxsize > 2 ** 32


ffibuilder = cffi.FFI()

# ----------- PPMd interfaces ---------------------
# 7zTypes.h
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
"""
)

ffibuilder.cdef(
    r"""
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

typedef struct {
    /* Inherits from IByteOut */
    void (*Write)(void *p, Byte b);
    void (*dst_write)(char *buf, int size, void *userdata);
    void *userdata;
} RawWriter;

typedef struct {
    /* Inherits from IByteIn */
    Byte (*Read)(void *p);
    int (*src_readinto)(char *buf, int size, void *userdata);
    void *userdata;
} RawReader;

extern "Python" int src_readinto(char *, int, void *);
extern "Python" void dst_write(char *, int, void *);

extern "Python" void *raw_alloc(size_t);
extern "Python" void raw_free(void *);

void ppmd_state_init(CPpmd7 *ppmd, unsigned int maxOrder, unsigned int memSize, ISzAlloc *allocator);
void ppmd_state_close(CPpmd7 *ppmd, ISzAlloc *allocator);
int ppmd_decompress_init(CPpmd7z_RangeDec *rc, RawReader *reader, int (*src_readingo)(char *, int, void*), void *userdata);
void ppmd_compress_init(CPpmd7z_RangeEnc *rc, RawWriter *write, void (*dst_write)(char *, int, void*), void *userdata);

void Ppmd7_Construct(CPpmd7 *p);
void Ppmd7_Init(CPpmd7 *p, unsigned maxOrder);
int Ppmd7_DecodeSymbol(CPpmd7 *p, CPpmd7z_RangeDec *rc);

void Ppmd7z_RangeEnc_Init(CPpmd7z_RangeEnc *p);
void Ppmd7z_RangeEnc_FlushData(CPpmd7z_RangeEnc *p);
void Ppmd7_EncodeSymbol(CPpmd7 *p, CPpmd7z_RangeEnc *rc, int symbol);

void ppmd8_malloc(CPpmd8 *p, unsigned int memSize, ISzAlloc *allocator);
void ppmd8_mfree(CPpmd8 *ppmd, ISzAlloc *allocator);
void ppmd8_decompress_init(CPpmd8 *p, RawReader *reader, int (*src_readinto)(char*, int, void*), void *userdata);
void ppmd8_compress_init(CPpmd8 *p, RawWriter *writer, void (*dst_write)(char*, int, void*), void *userdata);

void Ppmd8_Construct(CPpmd8 *p);
void Ppmd8_Init(CPpmd8 *p, unsigned maxOrder, unsigned restoreMethod);
void Ppmd8_EncodeSymbol(CPpmd8 *p, int symbol);
void Ppmd8_RangeEnc_FlushData(CPpmd8 *p);
Bool Ppmd8_RangeDec_Init(CPpmd8 *p);
int Ppmd8_DecodeSymbol(CPpmd8 *p);
"""
)

# ---------------------------------------------------------------------------
c_source = r"""
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
    void (*dst_write)(char *buf, int size, void *userdata);
    void *userdata;
} RawWriter;

typedef struct {
    /* Inherits from IByteIn */
    Byte (*Read)(void *p);
    int (*src_readinto)(char *buf, int size, void *userdata);
    void *userdata;
} RawReader;

static void Write(void *p, Byte b)
{
    RawWriter *bw = p;
    bw->dst_write(&b, 1, bw->userdata);
}

static Byte Read(void *p)
{
    RawReader *br = p;
    char b;
    int size = br->src_readinto(&b, 1, br->userdata);
    if (size <= 0)
        return 0;
    return (Byte) b;
}

void ppmd_state_init(CPpmd7 *p, unsigned int maxOrder, unsigned int memSize, ISzAlloc *allocator)
{
    Ppmd7_Construct(p);
    Ppmd7_Alloc(p, memSize, allocator);
    Ppmd7_Init(p, maxOrder);
}

void ppmd_state_close(CPpmd7 *ppmd, ISzAlloc *allocator)
{
    Ppmd7_Free(ppmd, allocator);
}

void ppmd_compress_init(CPpmd7z_RangeEnc *rc, RawWriter *writer,
                        void (*dst_write)(char*, int, void*), void *userdata)
{
    writer->Write = Write;
    writer->dst_write = dst_write;
    writer->userdata = userdata;
    rc->Stream = (IByteOut *) writer;
    Ppmd7z_RangeEnc_Init(rc);
}

int ppmd_decompress_init(CPpmd7z_RangeDec *rc, RawReader *reader,
                         int (*src_readinto)(char*, int, void*), void *userdata)
{
    reader->Read = Read;
    reader->src_readinto = src_readinto;
    reader->userdata = userdata;
    rc->Stream = (IByteIn *) reader;
    Bool res = Ppmd7z_RangeDec_Init(rc);
    return res;
}

void ppmd8_malloc(CPpmd8 *p, unsigned int memSize, ISzAlloc *allocator)
{
    Ppmd8_Alloc(p, memSize, allocator);
}

void ppmd8_mfree(CPpmd8 *ppmd, ISzAlloc *allocator)
{
    Ppmd8_Free(ppmd, allocator);
}

void ppmd8_compress_init(CPpmd8 *p, RawWriter *writer,
                         void (*dst_write)(char*, int, void*), void *userdata)
{
    writer->Write = Write;
    writer->dst_write = dst_write;
    writer->userdata = userdata;
    p->Stream.Out = (IByteOut *) writer;
}

void ppmd8_decompress_init(CPpmd8 *p, RawReader *reader,
                         int (*src_readinto)(char*, int, void*), void *userdata)
{
    reader->Read = Read;
    reader->src_readinto = src_readinto;
    reader->userdata = userdata;
    p->Stream.In = (IByteIn *) reader;
}
"""


def set_kwargs(**kwargs):
    ffibuilder.set_source(source=c_source, **kwargs)


if __name__ == "__main__":  # not when running with setuptools
    ffibuilder.compile(verbose=True)

/* 7zTypes.h -- minimal subset of 7-Zip's 7zTypes.h for pyppmd
   Only the definitions used by Ppmd.h / Ppmd7* / Ppmd8* of 7-Zip 26.05 are provided.
   Upstream: 7zTypes.h : Igor Pavlov : Public domain */

#ifndef ZIP7_7Z_TYPES_H
#define ZIP7_7Z_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifndef EXTERN_C_BEGIN
#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif
#endif

EXTERN_C_BEGIN

/* basic types */
typedef uint8_t  Byte;
typedef int16_t  Int16;
typedef uint16_t UInt16;
typedef int32_t  Int32;
typedef uint32_t UInt32;
typedef int64_t  Int64;
typedef uint64_t UInt64;

typedef int BoolInt;
#define True 1
#define False 0

/* alignment */
#ifdef _MSC_VER
  #define MY_ALIGN(n) __declspec(align(n))
#else
  #define MY_ALIGN(n) __attribute__ ((aligned(n)))
#endif

/* inline control */
#ifdef _MSC_VER
  #define Z7_NO_INLINE    __declspec(noinline)
  #define Z7_FORCE_INLINE __forceinline
#elif (defined(__GNUC__) && (__GNUC__ >= 4)) || (defined(__clang__) && (__clang_major__ >= 4))
  #define Z7_NO_INLINE    __attribute__((noinline))
  #define Z7_FORCE_INLINE __attribute__((always_inline)) inline
#else
  #define Z7_NO_INLINE
  #define Z7_FORCE_INLINE
#endif

/* C interfaces: the first parameter is a pointer to the interface structure itself */
#define Z7_C_IFACE_DECL(a) \
  struct a ## _; \
  typedef const struct a ## _ * a ## Ptr; \
  typedef struct a ## _ a; \
  struct a ## _

Z7_C_IFACE_DECL (IByteIn)
{
  Byte (*Read)(IByteInPtr p); /* reads one byte, returns 0 in case of EOF or error */
};
#define IByteIn_Read(p) (p)->Read(p)

Z7_C_IFACE_DECL (IByteOut)
{
  void (*Write)(IByteOutPtr p, Byte b);
};
#define IByteOut_Write(p, b) (p)->Write(p, b)

typedef struct ISzAlloc ISzAlloc;
typedef const ISzAlloc * ISzAllocPtr;

struct ISzAlloc
{
  void *(*Alloc)(ISzAllocPtr p, size_t size);
  void (*Free)(ISzAllocPtr p, void *address); /* address can be 0 */
};
#define ISzAlloc_Alloc(p, size) (p)->Alloc(p, size)
#define ISzAlloc_Free(p, a) (p)->Free(p, a)

EXTERN_C_END

#endif
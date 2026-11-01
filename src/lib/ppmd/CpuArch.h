/* CpuArch.h -- minimal subset of 7-Zip's CpuArch.h for pyppmd
   Ppmd code of 7-Zip 26.05 only needs:
     MY_CPU_SIZEOF_POINTER : (== 4) enables optional PPMD_32BIT mode (slightly faster)
     MY_CPU_BE             : selects internal layout of Successor field (either is correct)
     MY_CPU_pragma_pack_push_1 / MY_CPU_pragma_pop : pack(1) for PPMd context/state structs
   Upstream: CpuArch.h : Igor Pavlov : Public domain */

#ifndef ZIP7_INC_CPU_ARCH_H
#define ZIP7_INC_CPU_ARCH_H

#include "7zTypes.h"

#if defined(__SIZEOF_POINTER__)
  #define MY_CPU_SIZEOF_POINTER __SIZEOF_POINTER__
#elif defined(_WIN64)
  #define MY_CPU_SIZEOF_POINTER 8
#elif defined(_WIN32)
  #define MY_CPU_SIZEOF_POINTER 4
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  #define MY_CPU_BE
#endif

#ifdef _MSC_VER
  #define MY_CPU_pragma_pack_push_1   __pragma(pack(push, 1))
  #define MY_CPU_pragma_pop           __pragma(pack(pop))
#else
  #define MY_CPU_pragma_pack_push_1   _Pragma("pack(push, 1)")
  #define MY_CPU_pragma_pop           _Pragma("pack(pop)")
#endif

#endif
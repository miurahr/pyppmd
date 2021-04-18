//
// Created by miurahr on 2021/04/07.
// Based on CpuArch.h - 2017-07-17 : Igor Pavlov : Public domain
//

#ifndef PPMD_ARCH_H
#define PPMD_ARCH_H

#include <stddef.h>

typedef unsigned char Byte;
typedef short Int16;
typedef unsigned short UInt16;
typedef int Int32;
typedef unsigned int UInt32;

#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 Int64;
typedef unsigned __int64 UInt64;
#define UINT64_CONST(n) n
#else
typedef long long int Int64;
typedef unsigned long long int UInt64;
#define UINT64_CONST(n) n ## ULL
#endif

typedef int Bool;
#define True 1
#define False 0

#if  defined(_M_IX86) \
  || defined(__i386__) \
  || defined(_M_ARM) \
  || defined(_M_ARM_NT) \
  || defined(_M_ARMT) \
  || defined(__arm__) \
  || defined(__thumb__) \
  || defined(__ARMEL__) \
  || defined(__ARMEB__) \
  || defined(__THUMBEL__) \
  || defined(__THUMBEB__) \
  || defined(__mips__) \
  || defined(__ppc__) \
  || defined(__powerpc__) \
  || defined(__sparc__)
  #define PPMD_32BIT
#endif

#endif // PPMD_ARCH_H
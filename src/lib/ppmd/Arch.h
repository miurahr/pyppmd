//
// Created by miurahr on 2021/04/07.
// Based on CpuArch.h - 2017-07-17 : Igor Pavlov : Public domain
//

#ifndef PPMD_ARCH_H
#define PPMD_ARCH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t   Byte;
typedef  int16_t  Int16;
typedef uint16_t UInt16;
typedef  int32_t  Int32;
typedef uint32_t UInt32;
typedef  int64_t  Int64;
typedef uint64_t UInt64;
#define UINT64_CONST(n) UINT64_C(n)

typedef _Bool Bool;
#define True  true
#define False false

#if INTPTR_WIDTH == 32
  #define PPMD_32BIT
#endif

#endif // PPMD_ARCH_H

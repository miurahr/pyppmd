//
// Created by miurahr on 2021/04/07.
// Based on 7zTypes.h - 2017-07-17 : Igor Pavlov : Public domain
//

#ifndef PPMD_INTERFACE_H
#define PPMD_INTERFACE_H

/****************************
*  Streaming
****************************/

/* The following interfaces use first parameter as pointer to structure */

typedef struct IByteIn IByteIn;
struct IByteIn
{
    Byte (*Read)(const IByteIn *p); /* reads one byte, returns 0 in case of EOF or error */
};
#define IByteIn_Read(p) (p)->Read(p)

typedef struct IByteOut IByteOut;
struct IByteOut
{
    void (*Write)(const IByteOut *p, Byte b);
};
#define IByteOut_Write(p, b) (p)->Write(p, b)

typedef struct ISzAlloc ISzAlloc;
typedef const ISzAlloc * ISzAllocPtr;

struct ISzAlloc
{
    void *(*Alloc)(size_t size);
    void (*Free)(void *address); /* address can be NULL */
};

#define ISzAlloc_Alloc(p, size) (p)->Alloc(size)
#define ISzAlloc_Free(p, a) (p)->Free(a)

#endif //PPMD_INTERFACE_H

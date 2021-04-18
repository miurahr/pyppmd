/* pyppmd module for Python 3.6+
   ---
   Borrows BlocksOutputBuffer, unused data buffer functions
   from pyzstd module - BSD-3 licensed by Ma Lin.
   https://github.com/animalize/pyzstd
 */

#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "pythread.h"   /* For Python 3.6 */
#include "Ppmd7.h"
#include "Ppmd8.h"

#ifndef Py_UNREACHABLE
    #define Py_UNREACHABLE() assert(0)
#endif

/* ----------------------------
     BlocksOutputBuffer code
   ---------------------------- */
typedef struct {
    /* List of blocks */
    PyObject *list;
    /* Number of whole allocated size. */
    Py_ssize_t allocated;
    /* Max length of the buffer, negative number for unlimited length. */
    Py_ssize_t max_length;
} BlocksOutputBuffer;

typedef struct {
    /* Inherits from IByteOut */
    void (*Write)(void *p, Byte b);
    BlocksOutputBuffer * buffer;
    PPMD_outBuffer *outBuffer;
} BufferWriter;

typedef struct {
    /* Inherits from IByteIn */
    Byte (*Read)(void *p);
    PPMD_inBuffer *inBuffer;
} BufferReader;

static const char unable_allocate_msg[] = "Unable to allocate output buffer.";

/* Block size sequence. Below functions assume the type is int. */

#define KB (1024)
#define MB (1024*1024)
static const int BUFFER_BLOCK_SIZE[] =
        /* If change this list, also change:
             The CFFI implementation
             OutputBufferTestCase unittest
           If change the first blocks's size, also change:
             ZstdDecompressReader.seek() method
             ZstdFile.__init__() method
             ZstdFile.read1() method
             FileTestCase.test_decompress_limited() test */
        { 32*KB, 64*KB, 256*KB, 1*MB, 4*MB, 8*MB, 16*MB, 16*MB,
          32*MB, 32*MB, 32*MB, 32*MB, 64*MB, 64*MB, 128*MB, 128*MB,
          256*MB };

/* According to the block sizes defined by BUFFER_BLOCK_SIZE, the whole
   allocated size growth step is

1   32 KB       +32 KB
2   96 KB       +64 KB
3   352 KB      +256 KB
4   1.34 MB     +1 MB
5   5.34 MB     +4 MB
6   13.34 MB    +8 MB
7   29.34 MB    +16 MB
8   45.34 MB    +16 MB
9   77.34 MB    +32 MB
10  109.34 MB   +32 MB
11  141.34 MB   +32 MB
12  173.34 MB   +32 MB
13  237.34 MB   +64 MB
14  301.34 MB   +64 MB
15  429.34 MB   +128 MB
16  557.34 MB   +128 MB
17  813.34 MB   +256 MB
18  1069.34 MB  +256 MB
19  1325.34 MB  +256 MB
20  1581.34 MB  +256 MB
21  1837.34 MB  +256 MB
22  2093.34 MB  +256 MB
...
*/

/* Initialize the buffer, and grow the buffer.
   max_length: Max length of the buffer, -1 for unlimited length.
   Return 0 on success
   Return -1 on failure
*/
static inline int
OutputBuffer_InitAndGrow(BlocksOutputBuffer *buffer, PPMD_outBuffer *ob,
                         Py_ssize_t max_length)
{
    PyObject *b;
    int block_size;

    /* Set & check max_length */
    buffer->max_length = max_length;
    if (0 <= max_length && max_length < BUFFER_BLOCK_SIZE[0]) {
        block_size = (int) max_length;
    } else {
        block_size = BUFFER_BLOCK_SIZE[0];
    }

    /* The first block */
    b = PyBytes_FromStringAndSize(NULL, block_size);
    if (b == NULL) {
        buffer->list = NULL; /* For OutputBuffer_OnError() */
        return -1;
    }

    /* Create the list */
    buffer->list = PyList_New(1);
    if (buffer->list == NULL) {
        Py_DECREF(b);
        return -1;
    }
    PyList_SET_ITEM(buffer->list, 0, b);

    /* Set variables */
    buffer->allocated = block_size;

    ob->dst = PyBytes_AS_STRING(b);
    ob->size = block_size;
    ob->pos = 0;
    return 0;
}

/* Initialize the buffer, with an initial size.
   init_size: the initial size.
   Return 0 on success
   Return -1 on failure
*/
static inline int
OutputBuffer_InitWithSize(BlocksOutputBuffer *buffer, PPMD_outBuffer *ob,
                          Py_ssize_t init_size)
{
    PyObject *b;

    /* The first block */
    b = PyBytes_FromStringAndSize(NULL, init_size);
    if (b == NULL) {
        buffer->list = NULL; /* For OutputBuffer_OnError() */
        PyErr_SetString(PyExc_MemoryError, unable_allocate_msg);
        return -1;
    }

    /* Create the list */
    buffer->list = PyList_New(1);
    if (buffer->list == NULL) {
        Py_DECREF(b);
        return -1;
    }
    PyList_SET_ITEM(buffer->list, 0, b);

    /* Set variables */
    buffer->allocated = init_size;
    buffer->max_length = -1;

    ob->dst = PyBytes_AS_STRING(b);
    ob->size = (size_t) init_size;
    ob->pos = 0;
    return 0;
}

/* Grow the buffer. The avail_out must be 0, please check it before calling.
   Return 0 on success
   Return -1 on failure
*/
static inline int
OutputBuffer_Grow(BlocksOutputBuffer *buffer, PPMD_outBuffer *ob)
{
    PyObject *b;
    const Py_ssize_t list_len = Py_SIZE(buffer->list);
    int block_size;

    /* Ensure no gaps in the data */
    assert(ob->pos == ob->size);

    /* Get block size */
    if (list_len < (Py_ssize_t) Py_ARRAY_LENGTH(BUFFER_BLOCK_SIZE)) {
        block_size = BUFFER_BLOCK_SIZE[list_len];
    } else {
        block_size = BUFFER_BLOCK_SIZE[Py_ARRAY_LENGTH(BUFFER_BLOCK_SIZE) - 1];
    }

    /* Check max_length */
    if (buffer->max_length >= 0) {
        /* If (rest == 0), should not grow the buffer. */
        Py_ssize_t rest = buffer->max_length - buffer->allocated;
        assert(rest > 0);

        /* block_size of the last block */
        if (block_size > rest) {
            block_size = (int) rest;
        }
    }

    /* Check buffer->allocated overflow */
    if (block_size > PY_SSIZE_T_MAX - buffer->allocated) {
        PyErr_SetString(PyExc_MemoryError, unable_allocate_msg);
        return -1;
    }

    /* Create the block */
    b = PyBytes_FromStringAndSize(NULL, block_size);
    if (b == NULL) {
        PyErr_SetString(PyExc_MemoryError, unable_allocate_msg);
        return -1;
    }
    if (PyList_Append(buffer->list, b) < 0) {
        Py_DECREF(b);
        return -1;
    }
    Py_DECREF(b);

    /* Set variables */
    buffer->allocated += block_size;

    ob->dst = PyBytes_AS_STRING(b);
    ob->size = block_size;
    ob->pos = 0;
    return 0;
}

/* Whether the output data has reached max_length.
   The avail_out must be 0, please check it before calling. */
static inline int
OutputBuffer_ReachedMaxLength(BlocksOutputBuffer *buffer, PPMD_outBuffer *ob)
{
    /* Ensure (data size == allocated size) */
    assert(ob->pos == ob->size);

    return buffer->allocated == buffer->max_length;
}

/* Finish the buffer.
   Return a bytes object on success
   Return NULL on failure
*/
static PyObject *
OutputBuffer_Finish(BlocksOutputBuffer *buffer, PPMD_outBuffer *ob)
{
    PyObject *result, *block;
    const Py_ssize_t list_len = Py_SIZE(buffer->list);

    /* Fast path for single block */
    if ((list_len == 1 && ob->pos == ob->size) ||
        (list_len == 2 && ob->pos == 0))
    {
        block = PyList_GET_ITEM(buffer->list, 0);
        Py_INCREF(block);

        Py_DECREF(buffer->list);
        return block;
    }

    /* Final bytes object */
    result = PyBytes_FromStringAndSize(NULL, buffer->allocated - (ob->size - ob->pos));
    if (result == NULL) {
        PyErr_SetString(PyExc_MemoryError, unable_allocate_msg);
        return NULL;
    }

    /* Memory copy */
    if (list_len > 0) {
        char *posi = PyBytes_AS_STRING(result);

        /* Blocks except the last one */
        Py_ssize_t i = 0;
        for (; i < list_len-1; i++) {
            block = PyList_GET_ITEM(buffer->list, i);
            memcpy(posi, PyBytes_AS_STRING(block), Py_SIZE(block));
            posi += Py_SIZE(block);
        }
        /* The last block */
        block = PyList_GET_ITEM(buffer->list, i);
        memcpy(posi, PyBytes_AS_STRING(block), ob->pos);
    } else {
        /* buffer->list has at least one block, see initialize functions. */
        Py_UNREACHABLE();
    }

    Py_DECREF(buffer->list);
    return result;
}

/* Clean up the buffer */
static inline void
OutputBuffer_OnError(BlocksOutputBuffer *buffer)
{
    Py_XDECREF(buffer->list);
}

/* ------------------------------
   End of BlocksOutputBuffer code
   ------------------------------ */

static void Write(void *p, Byte b) {
    BufferWriter *bufferWriter = (BufferWriter *) p;

    // Grow output buffer size when buffer is full
    if (bufferWriter->outBuffer->size == bufferWriter->outBuffer->pos) {
        if (OutputBuffer_Grow(bufferWriter->buffer, bufferWriter->outBuffer) < 0) {
            // FIXME: propagate memory error to upstream
            goto error;
        }
    }

    *((Byte *)bufferWriter->outBuffer->dst + bufferWriter->outBuffer->pos++) = b;
    return;
error:
    return;
}

Byte Reader(void *p) {
    BufferReader *bufferReader = (BufferReader *) p;
    PPMD_inBuffer *inBuffer = bufferReader->inBuffer;
    if (inBuffer->pos == inBuffer->size) {
        return -1;
    } else {
        return *((const Byte *)inBuffer->src + inBuffer->pos++);
    }
}

static ISzAlloc allocator = {
    PyMem_Malloc,
    PyMem_Free
};

typedef struct {
    PyObject_HEAD

    /* Thread lock for compressing */
    PyThread_type_lock lock;

    /* Ppmd7 context */
    CPpmd7 *cPpmd7;

    /* RangeEncoder */
    CPpmd7z_RangeEnc *rangeEnc;

    /* __init__ has been called, 0 or 1. */
    char inited;
    /* flush() has been called, 0 or 1. */
    char flushed;
} Ppmd7Encoder;

typedef struct {
    PyObject_HEAD

    /* Unconsumed input data */
    char *input_buffer;
    size_t input_buffer_size;
    size_t in_begin, in_end;

    /* Thread lock for decompressing */
    PyThread_type_lock lock;

    /* Ppmd7 context */
    CPpmd7 *cPpmd7;

    /* Range Decoder */
    CPpmd7z_RangeDec *rangeDec;

    /* __init__ has been called, 0 or 1. */
    char inited;
    /* decode has been called with some data*/
    char inited2;
    /* flush has been called, 0 or 1 */
    char flushed;
} Ppmd7Decoder;

typedef struct {
    PyObject_HEAD

    /* Thread lock for compress */
    PyThread_type_lock lock;

    /* Ppmd8 context */
    CPpmd8 *cPpmd8;

    /* __init__ has been called, 0 or 1. */
    char inited;
    /* flush() has been called, 0 or 1. */
    char flushed;
} Ppmd8Encoder;

typedef struct {
    PyObject_HEAD

    /* Unconsumed input data */
    char *input_buffer;
    size_t input_buffer_size;
    size_t in_begin, in_end;

    /* Thread lock for decompressing */
    PyThread_type_lock lock;

    /* Ppmd7 context */
    CPpmd8 *cPpmd8;

    /* __init__ has been called, 0 or 1. */
    char inited;
    /* decode has been called with some data*/
    char inited2;
    /* flush has been called, 0 or 1 */
    char flushed;
} Ppmd8Decoder;

typedef struct {
    PyTypeObject *Ppmd7Encoder_type;
    PyTypeObject *Ppmd7Decoder_type;
    PyTypeObject *Ppmd8Encoder_type;
    PyTypeObject *Ppmd8Decoder_type;
    PyObject *PpmdError;
} _ppmd_state;

static _ppmd_state static_state;

#define ACQUIRE_LOCK(obj) do {                    \
    if (!PyThread_acquire_lock((obj)->lock, 0)) { \
        Py_BEGIN_ALLOW_THREADS                    \
        PyThread_acquire_lock((obj)->lock, 1);    \
        Py_END_ALLOW_THREADS                      \
    } } while (0)
#define RELEASE_LOCK(obj) PyThread_release_lock((obj)->lock)

static const char init_twice_msg[] = "__init__ method is called twice.";
static const char flush_twice_msg[] = "flush method is called twice.";

static inline void
clamp_max_order(unsigned long *max_order) {
    if (*max_order < 2) {
        *max_order = 2;
    } else if (*max_order > 64) {
        *max_order = 64;
    }
}

static inline void
clamp_memory_size(unsigned long *memorySize) {
    if (*memorySize < 1 << 11) {
        *memorySize = 1 << 11;
    } else if (*memorySize > 0xFFFFFFFF - 12 * 3) {
        *memorySize = 0xFFFFFFFF - 12 * 3;
    }
}

/* -----------------------
     Ppmd7Decoder code
   ------------------------ */
static PyObject *
Ppmd7Decoder_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Ppmd7Decoder *self;
    self = (Ppmd7Decoder*)type->tp_alloc(type, 0);
    if (self == NULL) {
        PyErr_NoMemory();
        goto error;
    }
    assert(self->inited == 0);
    assert(self->inited2 == 0);

    /* Thread lock */
    self->lock = PyThread_allocate_lock();
    if (self->lock == NULL) {
        PyErr_NoMemory();
        goto error;
    }
    return (PyObject*)self;

error:
    Py_XDECREF(self);
    return NULL;
}

static void
Ppmd7Decoder_dealloc(Ppmd7Decoder *self)
{
    if (self->lock) {
        PyThread_free_lock(self->lock);
    }
    Ppmd7_Free(self->cPpmd7, &allocator);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free((PyObject*)self);
    Py_DECREF(tp);
}

PyDoc_STRVAR(Ppmd7Decoder_doc, "A PPMd compression algorithm decoder.\n\n"
                                 "Ppmd7Decoder.__init__(self, max_order, mem_size)\n"
                                 "----\n"
                                 "Initialize a Ppmd7Decoder object.\n\n"
                                 "Arguments\n"
                                 "max_order: max order for the PPM modelling ranging from 2 to 64,\n"
                                 "           higher values produce better compression ratios but are slower.\n"
                                 "           Default is 6.\n"
                                 "mem_size:  max memory size in bytes the compressor is able to use, bigger values improve compression,\n"
                                 "           raging from 10kB to physical memory size.\n"
                                 "           Default size is 16MB.\n"
                                 );

static int
Ppmd7Decoder_init(Ppmd7Decoder *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"max_order", "mem_size", NULL};
    PyObject *max_order = Py_None;
    PyObject *mem_size = Py_None;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "OO:Ppmd7Decoder.__init__", kwlist,
                                     &max_order, &mem_size)) {
        return -1;
    }

    /* Only called once */
    if (self->inited) {
        PyErr_SetString(PyExc_RuntimeError, init_twice_msg);
        goto error;
    }
    self->inited = 1;

    unsigned long maximum_order = 6;
    unsigned long memory_size = 16 << 20;

    if (max_order != Py_None) {
        if (PyLong_Check(max_order)) {
            maximum_order = PyLong_AsUnsignedLong(max_order);
            if (maximum_order == (unsigned long)-1 && PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError,
                                "Max_order should be signed int value ranging from 2 to 16.");
                goto error;
            }
        }
        clamp_max_order(&maximum_order);
    }

    if (mem_size != Py_None) {
        if (PyLong_Check(mem_size)) {
            memory_size = PyLong_AsUnsignedLong(mem_size);
            if (memory_size == (unsigned long)-1 && PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError,
                                "Memory size should be unsigned long value.");
                goto error;
            }
        }
        clamp_memory_size(&memory_size);
    }

    if ((self->cPpmd7 =  PyMem_Malloc(sizeof(CPpmd7))) != NULL) {
        Ppmd7_Construct(self->cPpmd7);
        if (Ppmd7_Alloc(self->cPpmd7, (UInt32)memory_size, &allocator)) {
            Ppmd7_Init(self->cPpmd7, (unsigned int)maximum_order);
            if ((self->rangeDec = PyMem_Malloc(sizeof(CPpmd7z_RangeDec))) != NULL) {
                goto success;
            }
        }
        PyMem_Free(self->cPpmd7);
    }

error:
    return -1;

success:
    return 0;
}

PyDoc_STRVAR(Ppmd7Decoder_flush_doc, "flush()\n"
             "----\n"
             "A PPMd Ver.H decoder flush.");

static PyObject *
Ppmd7Decoder_flush(Ppmd7Decoder *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"length", NULL};
    BlocksOutputBuffer buffer;
    int length;
    PPMD_inBuffer in;
    PPMD_outBuffer out;
    PyObject *ret = NULL;
    BufferReader reader;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "i:Ppmd7Decoder.flush", kwlist,
                                     &length)) {
        return NULL;
    }

    /* Only called once */
    if (self->flushed) {
        PyErr_SetString(PyExc_RuntimeError, flush_twice_msg);
        goto error;
    }
    self->flushed = 1;

    if (self->inited2 == 0) {
       PyErr_SetString(PyExc_RuntimeError,
                       "Call flush() before calling decode()");
       return NULL;
    }

    ACQUIRE_LOCK(self);

    /* Prepare input buffer w/wo unconsumed data */
    if (self->in_begin == self->in_end) {
        /* No unconsumed data */
        char *tmp = PyMem_Malloc(0);
        if (tmp == NULL) {
            PyErr_NoMemory();
            RELEASE_LOCK(self);
            return NULL;
        }

        in.src = tmp;
        in.size = 0;
        in.pos = 0;
    } else {
        /* Has unconsumed data */
        assert(self->in_begin < self->in_end);
        in.src = self->input_buffer + self->in_begin;
        in.size = self->in_end - self->in_begin;
        in.pos = 0;
    }

    reader.Read = Reader;
    reader.inBuffer = &in;
    self->rangeDec->Stream = (IByteIn *) &reader;

    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        PyErr_SetString(PyExc_ValueError, "No Memory.");
        RELEASE_LOCK(self);
        return NULL;
    }

    for (int i = 0; i < length; i++) {
        if (out.pos == out.size) {
            if (OutputBuffer_Grow(&buffer, &out) < 0) {
                PyErr_SetString(PyExc_ValueError, "L603: Unknown status");
                goto error;
            }
        }
        *((Byte *)out.dst + out.pos++) = Ppmd7_DecodeSymbol(self->cPpmd7, self->rangeDec);
    }
    if (!Ppmd7z_RangeDec_IsFinishedOK(self->rangeDec)) {
        PyErr_SetString(PyExc_ValueError, "Decompression failed.");
        goto error;
    }
    ret = OutputBuffer_Finish(&buffer, &out);
    goto success;

error:
    Py_CLEAR(ret);

success:
    /* Clear input_buffer */
    self->in_begin = 0;
    self->in_end = 0;
    RELEASE_LOCK(self);
    return ret;
}

PyDoc_STRVAR(Ppmd7Decoder_decode_doc, "decode()\n"
             "----\n"
             "A PPMd compression decode.");

static PyObject *
Ppmd7Decoder_decode(Ppmd7Decoder *self,  PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"data", "length", NULL};
    BlocksOutputBuffer buffer;
    Py_buffer data;
    int length;
    PPMD_inBuffer in;
    PPMD_outBuffer out;
    PyObject *ret = NULL;
    char use_input_buffer;
    BufferReader reader;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "y*i:Ppmd7Decoder.decode", kwlist,
                                     &data, &length)) {
        return NULL;
    }

    if (self->inited2 == 0 && data.len < 5) {
       PyErr_SetString(PyExc_ValueError,
                       "Not enough data for starting decompression.");
       return NULL;
    }

    ACQUIRE_LOCK(self);

    /* Prepare input buffer w/wo unconsumed data */
    if (self->in_begin == self->in_end) {
        /* No unconsumed data */
        use_input_buffer = 0;

        in.src = data.buf;
        in.size = data.len;
        in.pos = 0;
    } else if (data.len == 0) {
        /* Has unconsumed data, fast path for b'' */
        assert(self->in_begin < self->in_end);

        use_input_buffer = 1;

        in.src = self->input_buffer + self->in_begin;
        in.size = self->in_end - self->in_begin;
        in.pos = 0;
    } else {
        /* Has unconsumed data */
        use_input_buffer = 1;

        /* Unconsumed data size in input_buffer */
        const size_t used_now = self->in_end - self->in_begin;
        assert(self->in_end > self->in_begin);

        /* Number of bytes we can append to input buffer */
        const size_t avail_now = self->input_buffer_size - self->in_end;
        assert(self->input_buffer_size >= self->in_end);

        /* Number of bytes we can append if we move existing contents to
           beginning of buffer */
        const size_t avail_total = self->input_buffer_size - used_now;
        assert(self->input_buffer_size >= used_now);

        if (avail_total < (size_t) data.len) {
            char *tmp;
            const size_t new_size = used_now + data.len;

            /* Allocate with new size */
            tmp = PyMem_Malloc(new_size);
            if (tmp == NULL) {
                PyErr_NoMemory();
                RELEASE_LOCK(self);
                return NULL;
            }

            /* Copy unconsumed data to the beginning of new buffer */
            memcpy(tmp,
                   self->input_buffer + self->in_begin,
                   used_now);

            /* Switch to new buffer */
            PyMem_Free(self->input_buffer);
            self->input_buffer = tmp;
            self->input_buffer_size = new_size;

            /* Set begin & end position */
            self->in_begin = 0;
            self->in_end = used_now;
        } else if (avail_now < (size_t) data.len) {
            /* Move unconsumed data to the beginning.
               dst < src, so using memcpy() is safe. */
            memcpy(self->input_buffer,
                   self->input_buffer + self->in_begin,
                   used_now);

            /* Set begin & end position */
            self->in_begin = 0;
            self->in_end = used_now;
        }

        /* Copy data to input buffer */
        memcpy(self->input_buffer + self->in_end, data.buf, data.len);
        self->in_end += data.len;
        in.src = self->input_buffer + self->in_begin;
        in.size = used_now + data.len;
        in.pos = 0;
    }
    assert(in.pos == 0);

    reader.Read = Reader;
    reader.inBuffer = &in;
    self->rangeDec->Stream = (IByteIn *) &reader;

    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        PyErr_SetString(PyExc_ValueError, "No Memory.");
        RELEASE_LOCK(self);
        return NULL;
    }

    if (self->inited2 == 0) {
        // first time initialized.
        assert(use_input_buffer == 0);
        if (!Ppmd7z_RangeDec_Init(self->rangeDec)) {
            RELEASE_LOCK(self);
            return NULL;
        } else {
            self->inited2 = 1;
        }
    }
    assert(self->inited2 == 1);

    if (data.len  > 0) {
        for (int i = 0; i < length; i++) {
            if (in.pos == in.size) {
                break;
            }
            if (out.pos == out.size) {
                if (OutputBuffer_Grow(&buffer, &out) < 0) {
                    PyErr_SetString(PyExc_ValueError, "L616: Unknown status");
                    goto error;
                }
            }
            *((Byte *)out.dst + out.pos++) = Ppmd7_DecodeSymbol(self->cPpmd7, self->rangeDec);
        }
    }

    ret = OutputBuffer_Finish(&buffer, &out);

    /* Unconsumed input data */
    if (in.pos == in.size) {
        if (use_input_buffer) {
            /* Clear input_buffer */
            self->in_begin = 0;
            self->in_end = 0;
        }
    } else {
        const size_t data_size = in.size - in.pos;

        if (!use_input_buffer) {
            /* Discard buffer if it's too small
               (resizing it may needlessly copy the current contents) */
            if (self->input_buffer != NULL &&
                self->input_buffer_size < data_size) {
                PyMem_Free(self->input_buffer);
                self->input_buffer = NULL;
                self->input_buffer_size = 0;
            }

            /* Allocate if necessary */
            if (self->input_buffer == NULL) {
                self->input_buffer = PyMem_Malloc(data_size);
                if (self->input_buffer == NULL) {
                    PyErr_NoMemory();
                    goto error;
                }
                self->input_buffer_size = data_size;
            }

            /* Copy unconsumed data */
            memcpy(self->input_buffer, (char*)in.src + in.pos, data_size);
            self->in_begin = 0;
            self->in_end = data_size;
        } else {
            /* Use input buffer */
            self->in_begin += in.pos;
        }
    }
    goto success;

error:
    /* Reset variables */
    self->in_begin = 0;
    self->in_end = 0;
    Py_CLEAR(ret);

success:
    RELEASE_LOCK(self);
    PyBuffer_Release(&data);
    return ret;
}

PyDoc_STRVAR(reduce_cannot_pickle_doc,
"Intentionally not supporting pickle.");

static PyObject *
reduce_cannot_pickle(PyObject *self)
{
    PyErr_Format(PyExc_TypeError,
                 "Cannot pickle %s object.",
                 Py_TYPE(self)->tp_name);
    return NULL;
}

static PyMethodDef Ppmd7Decoder_methods[] = {
        {"decode", (PyCFunction)Ppmd7Decoder_decode,
                     METH_VARARGS|METH_KEYWORDS, Ppmd7Decoder_decode_doc},
        {"flush", (PyCFunction)Ppmd7Decoder_flush,
                     METH_VARARGS|METH_KEYWORDS, Ppmd7Decoder_flush_doc},
        {"__reduce__", (PyCFunction)reduce_cannot_pickle,
                     METH_NOARGS, reduce_cannot_pickle_doc},
        {NULL, NULL, 0, NULL}
};

static PyType_Slot Ppmd7Decoder_slots[] = {
    {Py_tp_new, Ppmd7Decoder_new},
    {Py_tp_dealloc, Ppmd7Decoder_dealloc},
    {Py_tp_init, Ppmd7Decoder_init},
    {Py_tp_methods, Ppmd7Decoder_methods},
    {Py_tp_doc, Ppmd7Decoder_doc},
    {0, 0}
};

static PyType_Spec Ppmd7Decoder_type_spec = {
        .name = "_ppmd.Ppmd7Decoder",
        .basicsize = sizeof(Ppmd7Decoder),
        .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
        .slots = Ppmd7Decoder_slots,
};

/* -----------------------
     Ppmd7Encoder code
   ----------------------- */
static PyObject *
Ppmd7Encoder_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Ppmd7Encoder *self;
    if ((self = (Ppmd7Encoder*)type->tp_alloc(type, 0)) != NULL) {
        assert(self->inited == 0);
        /* Thread lock */
        if ((self->lock = PyThread_allocate_lock()) != NULL) {
            return (PyObject*)self;
        }
        Py_XDECREF(self);
    }
    PyErr_NoMemory();
    return NULL;
}

static void
Ppmd7Encoder_dealloc(Ppmd7Encoder *self)
{
    Ppmd7_Free(self->cPpmd7, &allocator);
    if (self->lock) {
        PyThread_free_lock(self->lock);
    }
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free((PyObject*)self);
    Py_DECREF(tp);
}

PyDoc_STRVAR(Ppmd7Encoder_doc, "A PPMd compression algorithm.\n\n"
                                 "Ppmd7Encoder.__init__(self, max_order, mem_size)\n"
                                 "----\n"
                                 "Initialize a Ppmd7Encoder object.\n\n"
                                 "Arguments\n"
                                 "max_order: max order for the PPM modelling ranging from 2 to 64, higher values produce better compression ratios but are slower.\n"
                                 "           Default is 6.\n"
                                 "mem_size:  max memory size in bytes the compressor is able to use, bigger values improve compression,\n"
                                 "           raging from 10kB to physical memory size.\n"
                                 "           Default size is 16MB.\n"
                                 );

static int
Ppmd7Encoder_init(Ppmd7Encoder *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"max_order", "mem_size", NULL};
    PyObject *max_order = Py_None;
    PyObject *mem_size = Py_None;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "OO:Ppmd7Encoder.__init__", kwlist,
                                     &max_order, &mem_size)) {
        goto error;
    }

    /* Only called once */
    if (self->inited) {
        PyErr_SetString(PyExc_RuntimeError, init_twice_msg);
        goto error;
    }
    self->inited = 1;

    unsigned long maximum_order = 6;
    unsigned long memory_size = 16 << 20;

    if (max_order != Py_None) {
        if (PyLong_Check(max_order)) {
            maximum_order = PyLong_AsUnsignedLong(max_order);
            if (maximum_order == (unsigned long)-1 && PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError,
                                "Max_order should be signed int value ranging from 2 to 16.");
                goto error;
            }
        }
        clamp_max_order(&maximum_order);
    }

    if (mem_size != Py_None) {
        if (PyLong_Check(mem_size)) {
            memory_size = PyLong_AsUnsignedLong(mem_size);
            if (memory_size == (unsigned long)-1 && PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError,
                                "Memory size should be unsigned long value.");
                goto error;
            }
        }
        clamp_memory_size(&memory_size);
    }

    if ((self->cPpmd7 =  PyMem_Malloc(sizeof(CPpmd7))) != NULL) {
        Ppmd7_Construct(self->cPpmd7);
        if (Ppmd7_Alloc(self->cPpmd7, (UInt32)memory_size, &allocator)) {
            Ppmd7_Init(self->cPpmd7, (unsigned int)maximum_order);
            if ((self->rangeEnc = PyMem_Malloc(sizeof(CPpmd7z_RangeEnc))) != NULL ) {
                Ppmd7z_RangeEnc_Init(self->rangeEnc);
                goto success;
            }
        }
        PyMem_Free(self->cPpmd7);
    }

error:
    return -1;

success:
    return 0;
}

PyDoc_STRVAR(Ppmd7Encoder_encode_doc, "encode()\n"
             "----\n"
             "A PPMd compression encode.");

static PyObject *
Ppmd7Encoder_encode(Ppmd7Encoder *self,  PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"data", NULL};
    BlocksOutputBuffer buffer;
    Py_buffer data;
    PyObject *ret;
    PPMD_outBuffer out;
    BufferWriter writer;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "y*:Ppmd7Encoder.encode", kwlist,
                                     &data)) {
        return NULL;
    }

    ACQUIRE_LOCK(self);
    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        PyErr_SetString(PyExc_ValueError, "No memory.");
        goto error;
    }

    writer.Write = Write;
    writer.outBuffer = &out;
    self->rangeEnc->Stream = (IByteOut *) &writer;

    for (UInt32 i = 0; i < data.len; i++){
        Ppmd7_EncodeSymbol(self->cPpmd7, self->rangeEnc, *((Byte *)data.buf + i));
        if (out.size == out.pos) {
            if (OutputBuffer_Grow(&buffer, &out) < 0) {
                PyErr_SetString(PyExc_ValueError, "No memory.");
                goto error;
            } else {
                writer.outBuffer = &out;
            }
        }
    }

    ret = OutputBuffer_Finish(&buffer, &out);
    RELEASE_LOCK(self);
    return ret;

error:
    OutputBuffer_OnError(&buffer);
    RELEASE_LOCK(self);
    return NULL;
}

PyDoc_STRVAR(Ppmd7Encoder_flush_doc, "flush()\n"
"----\n"
"Flush any remaining data in internal buffer.");

static PyObject *
Ppmd7Encoder_flush(Ppmd7Encoder *self, PyObject *args, PyObject *kwargs)
{
    PyObject *ret;
    CPpmd7z_RangeEnc *rc = self->rangeEnc;
    PPMD_outBuffer out;
    BlocksOutputBuffer buffer;
    BufferWriter writer;

    ACQUIRE_LOCK(self);

    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        PyErr_SetString(PyExc_ValueError, "No memory.");
        goto error;
    }

    writer.Write = Write;
    writer.outBuffer = &out;
    rc->Stream = (IByteOut *) &writer;

    Ppmd7z_RangeEnc_FlushData(rc);

    ret = OutputBuffer_Finish(&buffer, &out);

    RELEASE_LOCK(self);
    return ret;

error:
    OutputBuffer_OnError(&buffer);
    RELEASE_LOCK(self);
    return NULL;
}

static PyMethodDef Ppmd7Encoder_methods[] = {
        {"encode", (PyCFunction)Ppmd7Encoder_encode,
                     METH_VARARGS|METH_KEYWORDS, Ppmd7Encoder_encode_doc},
        {"flush", (PyCFunction)Ppmd7Encoder_flush,
                     METH_VARARGS|METH_KEYWORDS, Ppmd7Encoder_flush_doc},
        {"__reduce__", (PyCFunction)reduce_cannot_pickle,
                     METH_NOARGS, reduce_cannot_pickle_doc},
        {NULL, NULL, 0, NULL}
};

static PyType_Slot Ppmd7Encoder_slots[] = {
        {Py_tp_new, Ppmd7Encoder_new},
        {Py_tp_dealloc, Ppmd7Encoder_dealloc},
        {Py_tp_init, Ppmd7Encoder_init},
        {Py_tp_methods, Ppmd7Encoder_methods},
        {Py_tp_doc, Ppmd7Encoder_doc},
        {0, 0}
};

static PyType_Spec Ppmd7Encoder_type_spec = {
        .name = "_ppmd.Ppmd7Encoder",
        .basicsize = sizeof(Ppmd7Encoder),
        .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
        .slots = Ppmd7Encoder_slots,
};

/* -----------------------
     Ppmd8Decoder code
   ------------------------ */
static PyObject *
Ppmd8Decoder_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Ppmd8Decoder *self;
    self = (Ppmd8Decoder*)type->tp_alloc(type, 0);
    if (self == NULL) {
        PyErr_NoMemory();
        goto error;
    }
    assert(self->inited == 0);
    assert(self->inited2 == 0);

    /* Thread lock */
    self->lock = PyThread_allocate_lock();
    if (self->lock == NULL) {
        PyErr_NoMemory();
        goto error;
    }
    return (PyObject*)self;

error:
    Py_XDECREF(self);
    return NULL;
}

static void
Ppmd8Decoder_dealloc(Ppmd8Decoder *self)
{
    if (self->lock) {
        PyThread_free_lock(self->lock);
    }
    Ppmd8_Free(self->cPpmd8, &allocator);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free((PyObject*)self);
    Py_DECREF(tp);
}

PyDoc_STRVAR(Ppmd8Decoder_doc, "A PPMd compression algorithm decoder.\n\n"
                                 "Ppmd8Decoder.__init__(self, max_order, mem_size)\n"
                                 "----\n"
                                 "Initialize a Ppmd8Decoder object.\n\n"
                                 "Arguments\n"
                                 "max_order: max order for the PPM modelling ranging from 2 to 64,\n"
                                 "           higher values produce better compression ratios but are slower.\n"
                                 "           Default is 6.\n"
                                 "mem_size:  max memory size in bytes the compressor is able to use, bigger values improve compression,\n"
                                 "           raging from 10kB to physical memory size.\n"
                                 "           Default size is 16MB.\n"
                                 );

static int
Ppmd8Decoder_init(Ppmd8Decoder *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"max_order", "mem_size", NULL};
    PyObject *max_order = Py_None;
    PyObject *mem_size = Py_None;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "OO:Ppmd8Decoder.__init__", kwlist,
                                     &max_order, &mem_size)) {
        return -1;
    }

    /* Only called once */
    if (self->inited) {
        PyErr_SetString(PyExc_RuntimeError, init_twice_msg);
        goto error;
    }
    self->inited = 1;

    unsigned long maximum_order = 6;
    unsigned long memory_size = 16 << 20;

    if (max_order != Py_None) {
        if (PyLong_Check(max_order)) {
            maximum_order = PyLong_AsUnsignedLong(max_order);
            if (maximum_order == (unsigned long)-1 && PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError,
                                "Max_order should be signed int value ranging from 2 to 16.");
                goto error;
            }
        }
        clamp_max_order(&maximum_order);
    }

    if (mem_size != Py_None) {
        if (PyLong_Check(mem_size)) {
            memory_size = PyLong_AsUnsignedLong(mem_size);
            if (memory_size == (unsigned long)-1 && PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError,
                                "Memory size should be unsigned long value.");
                goto error;
            }
        }
        clamp_memory_size(&memory_size);
    }

    if ((self->cPpmd8 = PyMem_Malloc(sizeof(CPpmd8))) != NULL) {
        Ppmd8_Construct(self->cPpmd8);
        if (Ppmd8_Alloc(self->cPpmd8, (UInt32)memory_size, &allocator)) {
            Ppmd8_Init(self->cPpmd8, (unsigned int)maximum_order, PPMD8_RESTORE_METHOD_RESTART);
            goto success;
        }
        PyMem_Free(self->cPpmd8);
    }

error:
    return -1;

success:
    return 0;
}

PyDoc_STRVAR(Ppmd8Decoder_flush_doc, "flush()\n"
             "----\n"
             "A PPMd Ver.I decoder flush.");

static PyObject *
Ppmd8Decoder_flush(Ppmd8Decoder *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"length", NULL};
    int length;
    PPMD_inBuffer in;
    BufferReader reader;
    BlocksOutputBuffer buffer;
    PPMD_outBuffer out;
    PyObject *ret = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "i:Ppmd8Decoder.flush", kwlist,
                                     &length)) {
        return NULL;
    }

    /* Only called once */
    if (self->flushed) {
        PyErr_SetString(PyExc_RuntimeError, flush_twice_msg);
        goto error;
    }
    self->flushed = 1;

    if (self->inited2 == 0) {
       PyErr_SetString(PyExc_RuntimeError,
                       "Call flush() before calling decode()");
       return NULL;
    }

    ACQUIRE_LOCK(self);

    /* Prepare input buffer w/wo unconsumed data */
    if (self->in_begin == self->in_end) {
        /* No unconsumed data */
        char *tmp = PyMem_Malloc(0);
        if (tmp == NULL) {
            PyErr_NoMemory();
            RELEASE_LOCK(self);
            return NULL;
        }

        in.src = tmp;
        in.size = 0;
        in.pos = 0;
    } else {
        /* Has unconsumed data */
        assert(self->in_begin < self->in_end);
        in.src = self->input_buffer + self->in_begin;
        in.size = self->in_end - self->in_begin;
        in.pos = 0;
    }

    reader.Read = Reader;
    reader.inBuffer = &in;
    self->cPpmd8->Stream.In = (IByteIn *) &reader;

    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        PyErr_SetString(PyExc_ValueError, "No Memory.");
        RELEASE_LOCK(self);
        return NULL;
    }

    for (int i = 0; i < length; i++) {
        if (out.pos == out.size) {
            if (OutputBuffer_Grow(&buffer, &out) < 0) {
                PyErr_SetString(PyExc_ValueError, "L1328: Unknown status");
                goto error;
            }
        }
        *((Byte *)out.dst + out.pos++) = Ppmd8_DecodeSymbol(self->cPpmd8);
    }
    if (!self->cPpmd8->Code != 0) {
        PyErr_SetString(PyExc_ValueError, "Decompression failed.");
        goto error;
    }
    ret = OutputBuffer_Finish(&buffer, &out);
    goto success;

error:
    Py_CLEAR(ret);

success:
    /* Clear input_buffer */
    self->in_begin = 0;
    self->in_end = 0;
    RELEASE_LOCK(self);
    return ret;
}

PyDoc_STRVAR(Ppmd8Decoder_decode_doc, "decode()\n"
             "----\n"
             "A PPMd compression decode.");

static PyObject *
Ppmd8Decoder_decode(Ppmd8Decoder *self,  PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"data", "length", NULL};
    BlocksOutputBuffer buffer;
    Py_buffer data;
    int length;
    PPMD_inBuffer in;
    PPMD_outBuffer out;
    PyObject *ret = NULL;
    char use_input_buffer;
    BufferReader reader;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "y*i:Ppmd8Decoder.decode", kwlist,
                                     &data, &length)) {
        return NULL;
    }

    if (self->inited2 == 0 && data.len < 5) {
       PyErr_SetString(PyExc_ValueError,
                       "Not enough data for starting decompression.");
       return NULL;
    }

    ACQUIRE_LOCK(self);

    /* Prepare input buffer w/wo unconsumed data */
    if (self->in_begin == self->in_end) {
        /* No unconsumed data */
        use_input_buffer = 0;

        in.src = data.buf;
        in.size = data.len;
        in.pos = 0;
    } else if (data.len == 0) {
        /* Has unconsumed data, fast path for b'' */
        assert(self->in_begin < self->in_end);

        use_input_buffer = 1;

        in.src = self->input_buffer + self->in_begin;
        in.size = self->in_end - self->in_begin;
        in.pos = 0;
    } else {
        /* Has unconsumed data */
        use_input_buffer = 1;

        /* Unconsumed data size in input_buffer */
        const size_t used_now = self->in_end - self->in_begin;
        assert(self->in_end > self->in_begin);

        /* Number of bytes we can append to input buffer */
        const size_t avail_now = self->input_buffer_size - self->in_end;
        assert(self->input_buffer_size >= self->in_end);

        /* Number of bytes we can append if we move existing contents to
           beginning of buffer */
        const size_t avail_total = self->input_buffer_size - used_now;
        assert(self->input_buffer_size >= used_now);

        if (avail_total < (size_t) data.len) {
            char *tmp;
            const size_t new_size = used_now + data.len;

            /* Allocate with new size */
            tmp = PyMem_Malloc(new_size);
            if (tmp == NULL) {
                PyErr_NoMemory();
                RELEASE_LOCK(self);
                return NULL;
            }

            /* Copy unconsumed data to the beginning of new buffer */
            memcpy(tmp,
                   self->input_buffer + self->in_begin,
                   used_now);

            /* Switch to new buffer */
            PyMem_Free(self->input_buffer);
            self->input_buffer = tmp;
            self->input_buffer_size = new_size;

            /* Set begin & end position */
            self->in_begin = 0;
            self->in_end = used_now;
        } else if (avail_now < (size_t) data.len) {
            /* Move unconsumed data to the beginning.
               dst < src, so using memcpy() is safe. */
            memcpy(self->input_buffer,
                   self->input_buffer + self->in_begin,
                   used_now);

            /* Set begin & end position */
            self->in_begin = 0;
            self->in_end = used_now;
        }

        /* Copy data to input buffer */
        memcpy(self->input_buffer + self->in_end, data.buf, data.len);
        self->in_end += data.len;
        in.src = self->input_buffer + self->in_begin;
        in.size = used_now + data.len;
        in.pos = 0;
    }
    assert(in.pos == 0);

    reader.Read = Reader;
    reader.inBuffer = &in;
    self->cPpmd8->Stream.In = (IByteIn *) &reader;

    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        PyErr_SetString(PyExc_ValueError, "No Memory.");
        RELEASE_LOCK(self);
        return NULL;
    }

    if (self->inited2 == 0) {
        // first time initialized.
        assert(use_input_buffer == 0);
        if (!Ppmd8_RangeDec_Init(self->cPpmd8)) {
            RELEASE_LOCK(self);
            return NULL;
        } else {
            self->inited2 = 1;
        }
    }
    assert(self->inited2 == 1);

    if (data.len  > 0) {
        for (int i = 0; i < length; i++) {
            if (in.pos == in.size) {
                break;
            }
            if (out.pos == out.size) {
                if (OutputBuffer_Grow(&buffer, &out) < 0) {
                    PyErr_SetString(PyExc_ValueError, "L616: Unknown status");
                    goto error;
                }
            }
            *((Byte *)out.dst + out.pos++) = Ppmd8_DecodeSymbol(self->cPpmd8);
        }
    }

    ret = OutputBuffer_Finish(&buffer, &out);

    /* Unconsumed input data */
    if (in.pos == in.size) {
        if (use_input_buffer) {
            /* Clear input_buffer */
            self->in_begin = 0;
            self->in_end = 0;
        }
    } else {
        const size_t data_size = in.size - in.pos;

        if (!use_input_buffer) {
            /* Discard buffer if it's too small
               (resizing it may needlessly copy the current contents) */
            if (self->input_buffer != NULL &&
                self->input_buffer_size < data_size) {
                PyMem_Free(self->input_buffer);
                self->input_buffer = NULL;
                self->input_buffer_size = 0;
            }

            /* Allocate if necessary */
            if (self->input_buffer == NULL) {
                self->input_buffer = PyMem_Malloc(data_size);
                if (self->input_buffer == NULL) {
                    PyErr_NoMemory();
                    goto error;
                }
                self->input_buffer_size = data_size;
            }

            /* Copy unconsumed data */
            memcpy(self->input_buffer, (char*)in.src + in.pos, data_size);
            self->in_begin = 0;
            self->in_end = data_size;
        } else {
            /* Use input buffer */
            self->in_begin += in.pos;
        }
    }
    goto success;

error:
    /* Reset variables */
    self->in_begin = 0;
    self->in_end = 0;
    Py_CLEAR(ret);

success:
    RELEASE_LOCK(self);
    PyBuffer_Release(&data);
    return ret;
}

static PyMethodDef Ppmd8Decoder_methods[] = {
        {"decode", (PyCFunction)Ppmd8Decoder_decode,
                     METH_VARARGS|METH_KEYWORDS, Ppmd8Decoder_decode_doc},
        {"flush", (PyCFunction)Ppmd8Decoder_flush,
                     METH_VARARGS|METH_KEYWORDS, Ppmd8Decoder_flush_doc},
        {"__reduce__", (PyCFunction)reduce_cannot_pickle,
                     METH_NOARGS, reduce_cannot_pickle_doc},
        {NULL, NULL, 0, NULL}
};

static PyType_Slot Ppmd8Decoder_slots[] = {
    {Py_tp_new, Ppmd8Decoder_new},
    {Py_tp_dealloc, Ppmd8Decoder_dealloc},
    {Py_tp_init, Ppmd8Decoder_init},
    {Py_tp_methods, Ppmd8Decoder_methods},
    {Py_tp_doc, Ppmd8Decoder_doc},
    {0, 0}
};

static PyType_Spec Ppmd8Decoder_type_spec = {
        .name = "_ppmd.Ppmd8Decoder",
        .basicsize = sizeof(Ppmd8Decoder),
        .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
        .slots = Ppmd8Decoder_slots,
};

/* -----------------------
     Ppmd8Encoder code
   ----------------------- */
static PyObject *
Ppmd8Encoder_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Ppmd8Encoder *self;
    if ((self = (Ppmd8Encoder*)type->tp_alloc(type, 0)) != NULL) {
        assert(self->inited == 0);
        /* Thread lock */
        if ((self->lock = PyThread_allocate_lock()) != NULL) {
            return (PyObject*)self;
        }
        Py_XDECREF(self);
    }
    PyErr_NoMemory();
    return NULL;
}

static void
Ppmd8Encoder_dealloc(Ppmd8Encoder *self)
{
    Ppmd8_Free(self->cPpmd8, &allocator);
    if (self->lock) {
        PyThread_free_lock(self->lock);
    }
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free((PyObject*)self);
    Py_DECREF(tp);
}

PyDoc_STRVAR(Ppmd8Encoder_doc, "A PPMd compression algorithm.\n\n"
                                 "Ppmd8Encoder.__init__(self, max_order, mem_size)\n"
                                 "----\n"
                                 "Initialize a Ppmd8Encoder object.\n\n"
                                 "Arguments\n"
                                 "max_order: max order for the PPM modelling ranging from 2 to 64, higher values produce better compression ratios but are slower.\n"
                                 "           Default is 6.\n"
                                 "mem_size:  max memory size in bytes the compressor is able to use, bigger values improve compression,\n"
                                 "           raging from 10kB to physical memory size.\n"
                                 "           Default size is 16MB.\n"
                                 );

static int
Ppmd8Encoder_init(Ppmd8Encoder *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"max_order", "mem_size", NULL};
    PyObject *max_order = Py_None;
    PyObject *mem_size = Py_None;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "OO:Ppmd8Encoder.__init__", kwlist,
                                     &max_order, &mem_size)) {
        goto error;
    }

    /* Only called once */
    if (self->inited) {
        PyErr_SetString(PyExc_RuntimeError, init_twice_msg);
        goto error;
    }
    self->inited = 1;

    unsigned long maximum_order = 6;
    unsigned long memory_size = 16 << 20;

    if (max_order != Py_None) {
        if (PyLong_Check(max_order)) {
            maximum_order = PyLong_AsUnsignedLong(max_order);
            if (maximum_order == (unsigned long)-1 && PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError,
                                "Max_order should be signed int value ranging from 2 to 16.");
                goto error;
            }
        }
        clamp_max_order(&maximum_order);
    }

    if (mem_size != Py_None) {
        if (PyLong_Check(mem_size)) {
            memory_size = PyLong_AsUnsignedLong(mem_size);
            if (memory_size == (unsigned long)-1 && PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError,
                                "Memory size should be unsigned long value.");
                goto error;
            }
        }
        clamp_memory_size(&memory_size);
    }

    if ((self->cPpmd8 =  PyMem_Malloc(sizeof(CPpmd8))) != NULL) {
        Ppmd8_Construct(self->cPpmd8);
        if (Ppmd8_Alloc(self->cPpmd8, (UInt32)memory_size, &allocator)) {
            Ppmd8_RangeEnc_Init(self->cPpmd8);
            Ppmd8_Init(self->cPpmd8, (unsigned int)maximum_order, PPMD8_RESTORE_METHOD_RESTART);
            goto success;
        }
        PyMem_Free(self->cPpmd8);
    }
error:
    return -1;

success:
    return 0;
}

PyDoc_STRVAR(Ppmd8Encoder_encode_doc, "encode()\n"
             "----\n"
             "A PPMd compression encode.");

static PyObject *
Ppmd8Encoder_encode(Ppmd8Encoder *self,  PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"data", NULL};
    BlocksOutputBuffer buffer;
    Py_buffer data;
    PyObject *ret;
    PPMD_outBuffer out;
    BufferWriter writer;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "y*:Ppmd8Encoder.encode", kwlist,
                                     &data)) {
        return NULL;
    }

    ACQUIRE_LOCK(self);
    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        PyErr_SetString(PyExc_ValueError, "No memory.");
        goto error;
    }

    writer.Write = Write;
    writer.outBuffer = &out;
    self->cPpmd8->Stream.Out = (IByteOut *) &writer;

    for (UInt32 i = 0; i < data.len; i++){
        Ppmd8_EncodeSymbol(self->cPpmd8, *((Byte *)data.buf + i));
        if (out.size == out.pos) {
            if (OutputBuffer_Grow(&buffer, &out) < 0) {
                PyErr_SetString(PyExc_ValueError, "No memory.");
                goto error;
            } else {
                writer.outBuffer = &out;
            }
        }
    }

    ret = OutputBuffer_Finish(&buffer, &out);
    RELEASE_LOCK(self);
    return ret;

error:
    OutputBuffer_OnError(&buffer);
    RELEASE_LOCK(self);
    return NULL;
}

PyDoc_STRVAR(Ppmd8Encoder_flush_doc, "flush()\n"
"----\n"
"Flush any remaining data in internal buffer.");

static PyObject *
Ppmd8Encoder_flush(Ppmd8Encoder *self, PyObject *args, PyObject *kwargs)
{
    PyObject *ret;
    PPMD_outBuffer out;
    BlocksOutputBuffer buffer;
    BufferWriter writer;

    ACQUIRE_LOCK(self);

    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        PyErr_SetString(PyExc_ValueError, "No memory.");
        goto error;
    }

    writer.Write = Write;
    writer.outBuffer = &out;
    self->cPpmd8->Stream.Out = (IByteOut *) &writer;
    Ppmd8_EncodeSymbol(self->cPpmd8, -1);  // endmark
    Ppmd8_RangeEnc_FlushData(self->cPpmd8);
    ret = OutputBuffer_Finish(&buffer, &out);

    RELEASE_LOCK(self);
    return ret;

error:
    OutputBuffer_OnError(&buffer);
    RELEASE_LOCK(self);
    return NULL;
}

static PyMethodDef Ppmd8Encoder_methods[] = {
        {"encode", (PyCFunction)Ppmd8Encoder_encode,
                     METH_VARARGS|METH_KEYWORDS, Ppmd8Encoder_encode_doc},
        {"flush", (PyCFunction)Ppmd8Encoder_flush,
                     METH_VARARGS|METH_KEYWORDS, Ppmd8Encoder_flush_doc},
        {"__reduce__", (PyCFunction)reduce_cannot_pickle,
                     METH_NOARGS, reduce_cannot_pickle_doc},
        {NULL, NULL, 0, NULL}
};

static PyType_Slot Ppmd8Encoder_slots[] = {
        {Py_tp_new, Ppmd8Encoder_new},
        {Py_tp_dealloc, Ppmd8Encoder_dealloc},
        {Py_tp_init, Ppmd8Encoder_init},
        {Py_tp_methods, Ppmd8Encoder_methods},
        {Py_tp_doc, Ppmd8Encoder_doc},
        {0, 0}
};

static PyType_Spec Ppmd8Encoder_type_spec = {
        .name = "_ppmd.Ppmd8Encoder",
        .basicsize = sizeof(Ppmd8Encoder),
        .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
        .slots = Ppmd8Encoder_slots,
};

/* --------------------
     Initialize code
   -------------------- */

static PyMethodDef _ppmd_methods[] = {
    {NULL}
};

static int
_ppmd_traverse(PyObject *module, visitproc visit, void *arg)
{
    Py_VISIT(static_state.PpmdError);
    Py_VISIT(static_state.Ppmd7Encoder_type);
    Py_VISIT(static_state.Ppmd7Decoder_type);
    Py_VISIT(static_state.Ppmd8Encoder_type);
    Py_VISIT(static_state.Ppmd8Decoder_type);
    return 0;
}

static int
_ppmd_clear(PyObject *module)
{
    Py_CLEAR(static_state.PpmdError);
    Py_CLEAR(static_state.Ppmd7Encoder_type);
    Py_CLEAR(static_state.Ppmd7Decoder_type);
    Py_CLEAR(static_state.Ppmd8Encoder_type);
    Py_CLEAR(static_state.Ppmd8Decoder_type);
    return 0;
}

static void
_ppmd_free(void *module) {
    _ppmd_clear((PyObject *)module);
}

static PyModuleDef _ppmdmodule = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_ppmd",
    .m_size = -1,
    .m_methods = _ppmd_methods,
    .m_traverse = _ppmd_traverse,
    .m_clear = _ppmd_clear,
    .m_free = _ppmd_free
};


static inline int
add_type_to_module(PyObject *module, const char *name,
                   PyType_Spec *type_spec, PyTypeObject **dest)
{
    PyObject *temp;

    temp = PyType_FromSpec(type_spec);
    if (PyModule_AddObject(module, name, temp) < 0) {
        Py_XDECREF(temp);
        return -1;
    }

    Py_INCREF(temp);
    *dest = (PyTypeObject*) temp;

    return 0;
}

PyMODINIT_FUNC
PyInit__ppmd(void) {
    PyObject *module;

    module = PyModule_Create(&_ppmdmodule);
    if (!module) {
        goto error;
    }
    if (add_type_to_module(module,
                           "Ppmd7Encoder",
                           &Ppmd7Encoder_type_spec,
                           &static_state.Ppmd7Encoder_type) < 0) {
        goto error;
    }
    if (add_type_to_module(module,
                           "Ppmd7Decoder",
                           &Ppmd7Decoder_type_spec,
                           &static_state.Ppmd7Decoder_type) < 0) {
        goto error;
    }
    if (add_type_to_module(module,
                           "Ppmd8Encoder",
                           &Ppmd8Encoder_type_spec,
                           &static_state.Ppmd8Encoder_type) < 0) {
        goto error;
    }
    if (add_type_to_module(module,
                           "Ppmd8Decoder",
                           &Ppmd8Decoder_type_spec,
                           &static_state.Ppmd8Decoder_type) < 0) {
        goto error;
    }
    return module;

error:
     _ppmd_clear(NULL);
     Py_XDECREF(module);

     return NULL;
}

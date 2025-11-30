/* pyppmd module for Python 3.6+
   ---
   Borrows BlocksOutputBuffer, unused data buffer functions
   from pyzstd module - BSD-3 licensed by Ma Lin.
   https://github.com/animalize/pyzstd
 */

#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "pythread.h"   /* For Python 3.6 */
#include "structmember.h"

#if defined(_WIN32) && defined(timezone)
#undef timezone
#endif

#include "Ppmd7.h"
#include "Ppmd8.h"
#include "Ppmd7Dec2.h"

#include "Buffer.h"
#include "ThreadDecoder.h"

#ifndef Py_UNREACHABLE
    #define Py_UNREACHABLE() assert(0)
#endif

#include "blockoutput.h"

static IAlloc allocator = {
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

    /* Output Buffer */
    BlocksOutputBuffer *blocksOutputBuffer;

    /* Unused data */
    PyObject *unused_data;

    /* 0 if decompressor has (or may has) unconsumed input data, 0 or 1. */
    char needs_input;

    /* 1 when end mark observed */
    char eof;

    OutBuffer *out;

    /* __init__ has been called, 0 or 1. */
    char inited;
    /* decode has been called with some data*/
    char inited2;
} Ppmd7Decoder;

/* Push-based Ppmd7 decoder (t for transfer/push) */
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

    /* Push Range Decoder */
    CPpmd7t_RangeDec rc;

    /* Output Buffer */
    BlocksOutputBuffer *blocksOutputBuffer;

    /* Unused data */
    PyObject *unused_data;

    /* 0 if decompressor has (or may has) unconsumed input data, 0 or 1. */
    char needs_input;

    /* 1 when end mark observed */
    char eof;

    OutBuffer *out;

    /* __init__ has been called, 0 or 1. */
    char inited;
    /* decode has been called with some data*/
    char inited2;
    /* Pending output ring (simple linear buffer) for bytes decoded beyond
       the requested length in order to detect EOF exactly */
    char *pending_buf;
    size_t pending_size;  /* number of valid bytes in pending */
    size_t pending_pos;   /* read position within pending */
    size_t pending_cap;   /* allocated capacity */
} Ppmd7tDecoder;

/*
 * Common utility definitions
 */

static const char init_twice_msg[] = "__init__ method is called twice.";
static const char flush_twice_msg[] = "flush method is called twice.";

static void clamp_max_order(unsigned long *max_order, unsigned long max) {
    assert(PPMD7_MIN_ORDER == PPMD8_MIN_ORDER);
    if (*max_order < PPMD7_MIN_ORDER) {
        *max_order = PPMD7_MIN_ORDER;
    } else if (*max_order > max) {
        *max_order = max;
    }
}

static void clamp_memory_size(unsigned long *memorySize) {
    if (*memorySize < PPMD7_MIN_MEM_SIZE) {
        *memorySize = PPMD7_MIN_MEM_SIZE;
    } else if (*memorySize > PPMD7_MAX_MEM_SIZE) {
        *memorySize = PPMD7_MAX_MEM_SIZE;
    }
}

/* -----------------------
     Ppmd7tDecoder code
   ------------------------ */
static PyObject *
Ppmd7tDecoder_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Ppmd7tDecoder *self = (Ppmd7tDecoder*)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;
    assert(self->inited == 0);
    assert(self->inited2 == 0);

    /* Thread lock */
    self->lock = PyThread_allocate_lock();
    if (self->lock == NULL) {
        Py_DECREF(self);
        return PyErr_NoMemory();
    }

    self->cPpmd7 = NULL;
    self->blocksOutputBuffer = NULL;
    self->unused_data = NULL;
    self->needs_input = 1;
    self->eof = 0;
    self->out = NULL;
    self->input_buffer = NULL;
    self->input_buffer_size = 0;
    self->in_begin = self->in_end = 0;
    Ppmd7t_RangeDec_Reset(&self->rc);
    self->pending_buf = NULL;
    self->pending_size = 0;
    self->pending_pos = 0;
    self->pending_cap = 0;
    return (PyObject*)self;
}

static void
Ppmd7tDecoder_dealloc(Ppmd7tDecoder *self)
{
    if (self->lock) {
        PyThread_free_lock(self->lock);
    }
    if (self->cPpmd7 != NULL) {
        Ppmd7_Free(self->cPpmd7, &allocator);
        PyMem_Free(self->cPpmd7);
    }
    if (self->blocksOutputBuffer) {
        if (self->out) {
            PyMem_Free(self->out);
        }
        PyMem_Free(self->blocksOutputBuffer);
    }
    if (self->input_buffer) {
        PyMem_Free(self->input_buffer);
    }
    if (self->pending_buf) {
        PyMem_Free(self->pending_buf);
    }
    Py_XDECREF(self->unused_data);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free((PyObject*)self);
    Py_DECREF(tp);
}

PyDoc_STRVAR(Ppmd7tDecoder_doc, "A PPMd7 decoder (push/buffer-driven).\n\n"
                                 "Ppmd7tDecoder.__init__(self, max_order, mem_size)\n");

static int
Ppmd7tDecoder_init(Ppmd7tDecoder *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"max_order", "mem_size", NULL};
    PyObject *max_order = Py_None;
    PyObject *mem_size = Py_None;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "OO:Ppmd7tDecoder.__init__", kwlist,
                                     &max_order, &mem_size)) {
        return -1;
    }
    if (self->inited) {
        PyErr_SetString(PyExc_RuntimeError, init_twice_msg);
        return -1;
    }

    unsigned long maxOrder = (max_order == Py_None) ? 6 : PyLong_AsUnsignedLong(max_order);
    if (PyErr_Occurred()) return -1;
    unsigned long memSize = (mem_size == Py_None) ? (16ul * 1024 * 1024) : PyLong_AsUnsignedLong(mem_size);
    if (PyErr_Occurred()) return -1;
    clamp_max_order(&maxOrder, PPMD7_MAX_ORDER);
    clamp_memory_size(&memSize);

    self->cPpmd7 = PyMem_Malloc(sizeof(CPpmd7));
    if (!self->cPpmd7)
        return -1;
    Ppmd7_Construct(self->cPpmd7);
    if (!Ppmd7_Alloc(self->cPpmd7, (UInt32)memSize, &allocator)) {
        PyMem_Free(self->cPpmd7);
        self->cPpmd7 = NULL;
        PyErr_NoMemory();
        return -1;
    }
    Ppmd7_Init(self->cPpmd7, (unsigned)maxOrder);

    self->blocksOutputBuffer = PyMem_Malloc(sizeof(BlocksOutputBuffer));
    self->out = PyMem_Malloc(sizeof(OutBuffer));
    if (!self->blocksOutputBuffer || !self->out) {
        if (self->blocksOutputBuffer) PyMem_Free(self->blocksOutputBuffer);
        if (self->out) PyMem_Free(self->out);
        Ppmd7_Free(self->cPpmd7, &allocator);
        PyMem_Free(self->cPpmd7);
        self->cPpmd7 = NULL;
        PyErr_NoMemory();
        return -1;
    }
    if (OutputBuffer_InitAndGrow(self->blocksOutputBuffer, self->out, -1) < 0) {
        PyMem_Free(self->blocksOutputBuffer);
        PyMem_Free(self->out);
        self->blocksOutputBuffer = NULL;
        self->out = NULL;
        Ppmd7_Free(self->cPpmd7, &allocator);
        PyMem_Free(self->cPpmd7);
        self->cPpmd7 = NULL;
        return -1;
    }

    self->eof = 0;
    self->needs_input = 1;
    self->inited = 1;
    self->inited2 = 0;
    return 0;
}

static PyObject *
Ppmd7t_unused_data_get(Ppmd7tDecoder *self, void *Py_UNUSED(ignored))
{
    PyObject *ret;
    if (!self->eof) {
        ret = PyBytes_FromStringAndSize(NULL, 0);
    } else {
        if (self->unused_data == NULL) {
            self->unused_data = PyBytes_FromStringAndSize(
                    self->input_buffer + self->in_begin,
                    self->in_end - self->in_begin);
            ret = self->unused_data;
            Py_XINCREF(ret);
        } else {
            ret = self->unused_data;
            Py_INCREF(ret);
        }
    }
    return ret;
}

PyDoc_STRVAR(Ppmd7tDecoder_decode_doc, "decode(data, length) -> bytes\n\n"
             "Feed input bytes and decode up to length into output.");

static PyObject *
Ppmd7tDecoder_decode(Ppmd7tDecoder *self,  PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"data", "length", NULL};
    Py_buffer data;
    int length;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "y*i:Ppmd7tDecoder.decode", kwlist,
                                     &data, &length)) {
        return NULL;
    }

    if (self->inited2 == 0 && data.len < 5) {
       PyErr_SetString(PyExc_ValueError,
                       "Not enough data for starting decompression.");
       return NULL;
    }

    /* setup output aggregation */
    BlocksOutputBuffer *blocks = self->blocksOutputBuffer;
    OutBuffer *out = self->out;

    PyObject *result = NULL;
    size_t total_written = 0;
    int want = (length < 0) ? 0x7fffffff : length;
    self->needs_input = 0;

    /* Build a contiguous input view. Reuse existing input_buffer when needed. */
    const Byte *in_ptr = NULL;
    size_t in_size = 0;
    int using_internal = 0;
    if (self->in_begin == self->in_end) {
        /* no unconsumed */
        in_ptr = (const Byte*)data.buf;
        in_size = (size_t)data.len;
    } else if (data.len == 0) {
        using_internal = 1;
        in_ptr = (Byte*)self->input_buffer + self->in_begin;
        in_size = self->in_end - self->in_begin;
    } else {
        /* combine existing unconsumed + new into internal buffer */
        const size_t used_now = self->in_end - self->in_begin;
        const size_t avail_total = (self->input_buffer_size >= used_now) ? (self->input_buffer_size - used_now) : 0;
        if (avail_total < (size_t)data.len) {
            size_t new_size = used_now + data.len;
            char *tmp = PyMem_Realloc(self->input_buffer, new_size);
            if (!tmp) {
                PyErr_NoMemory();
                goto done;
            }
            self->input_buffer = tmp;
            self->input_buffer_size = new_size;
        }
        memmove(self->input_buffer, self->input_buffer + self->in_begin, used_now);
        memcpy(self->input_buffer + used_now, data.buf, data.len);
        self->in_begin = 0;
        self->in_end = used_now + data.len;
        using_internal = 1;
        in_ptr = (Byte*)self->input_buffer;
        in_size = self->in_end;
    }

    if (!self->inited2) {
        self->inited2 = 1;
    }

    /* decoding loop */
    for (;;) {
        if (out->pos >= out->size) {
            if (OutputBuffer_Grow(blocks, out) < 0) {
                goto done;
            }
        }
        size_t out_cap = out->size - out->pos;
        if (want >= 0 && (size_t)want - total_written < out_cap)
            out_cap = (size_t)want - total_written;
        /* First, drain any pending output from previous call */
        if (out_cap > 0 && self->pending_size > self->pending_pos) {
            size_t avail = self->pending_size - self->pending_pos;
            if (avail > out_cap) avail = out_cap;
            memcpy((Byte*)out->dst + out->pos,
                   self->pending_buf + self->pending_pos, avail);
            self->pending_pos += avail;
            out->pos += avail;
            total_written += avail;
            if (self->pending_pos == self->pending_size) {
                self->pending_pos = self->pending_size = 0; /* fully drained */
            }
            continue;
        }
        if (out_cap == 0)
            break;

        Ppmd7t_RangeDec_SetInput(&self->rc, in_ptr, in_size);
        size_t out_written = 0, in_consumed = 0;
        int finished_ok = 0;
        Ppmd7tStatus st = Ppmd7t_Decode(self->cPpmd7, &self->rc,
                                        (Byte*)out->dst + out->pos, out_cap,
                                        &out_written, &in_consumed, &finished_ok);
        out->pos += out_written;
        total_written += out_written;

        /* advance input */
        if (using_internal) {
            self->in_begin += in_consumed;
            if (self->in_begin > self->in_end) self->in_begin = self->in_end; /* safety */
            in_ptr = (Byte*)self->input_buffer + self->in_begin;
            in_size = self->in_end - self->in_begin;
        } else {
            /* direct data buffer */
            in_ptr += in_consumed;
            in_size -= in_consumed;
        }

        if (st == PPMD7T_STATUS_OK) {
            if ((size_t)want == total_written)
                break;
            continue;
        } else if (st == PPMD7T_STATUS_NEED_INPUT) {
            self->needs_input = 1;
            break;
        } else if (st == PPMD7T_STATUS_END) {
            self->eof = 1;
            /* keep unused data for property */
            if (!using_internal) {
                /* copy remaining to internal buffer */
                size_t rem = in_size;
                if (rem) {
                    if (self->input_buffer_size < rem) {
                        char *tmp = PyMem_Realloc(self->input_buffer, rem);
                        if (!tmp) { PyErr_NoMemory(); goto done; }
                        self->input_buffer = tmp;
                        self->input_buffer_size = rem;
                    }
                    memcpy(self->input_buffer, in_ptr, rem);
                }
                self->in_begin = 0;
                self->in_end = rem;
            }
            break;
        } else {
            PyErr_SetString(PyExc_ValueError, "Corrupted input data");
            goto done;
        }
    }

    /* If we produced exactly what was requested but haven't reached EOF or NEED_INPUT,
       continue decoding into an internal pending buffer until END or NEED_INPUT.
       This allows us to set eof flag accurately without returning extra bytes. */
    if (!self->eof && !self->needs_input && (want >= 0) && total_written == (size_t)want) {
        /* Continue decoding internally to determine END/NEED_INPUT without
           returning more output. Add safety to avoid infinite loops. */
        size_t safety_iters = 0;
        for (;;) {
            /* ensure pending capacity */
            size_t chunk = 4096;
            size_t old_size = self->pending_size;
            size_t need = old_size + chunk;
            if (self->pending_cap < need) {
                size_t new_cap = self->pending_cap ? self->pending_cap : 4096;
                while (new_cap < need) new_cap *= 2;
                char *tmp = PyMem_Realloc(self->pending_buf, new_cap);
                if (!tmp) { PyErr_NoMemory(); goto done; }
                self->pending_buf = tmp; self->pending_cap = new_cap;
            }
            Ppmd7t_RangeDec_SetInput(&self->rc, in_ptr, in_size);
            size_t ow = 0, ic = 0; int finished_ok = 0;
            Ppmd7tStatus st = Ppmd7t_Decode(self->cPpmd7, &self->rc,
                                            (Byte*)self->pending_buf + old_size, chunk,
                                            &ow, &ic, &finished_ok);
            /* advance input */
            if (using_internal) {
                self->in_begin += ic;
                if (self->in_begin > self->in_end) self->in_begin = self->in_end;
                in_ptr = (Byte*)self->input_buffer + self->in_begin;
                in_size = self->in_end - self->in_begin;
            } else {
                in_ptr += ic;
                in_size -= ic;
            }
            self->pending_size = old_size + ow;
            if (st == PPMD7T_STATUS_OK) {
                /* If no progress and no input, declare NEED_INPUT to avoid spinning */
                if (ow == 0) {
                    if (in_size == 0) {
                        self->needs_input = 1;
                        break;
                    }
                }
                /* continue probing */
                ;
            } else if (st == PPMD7T_STATUS_END) {
                self->eof = 1;
                break;
            } else if (st == PPMD7T_STATUS_NEED_INPUT) {
                self->needs_input = 1;
                break;
            } else {
                PyErr_SetString(PyExc_ValueError, "Corrupted input data");
                goto done;
            }
            if (++safety_iters > 100000) { /* hard safety break */
                self->needs_input = 1;
                break;
            }
        }
    }

    result = OutputBuffer_Finish(blocks, out);

done:
    /* Preserve any unconsumed input for next calls */
    if (result != NULL) {
        if (in_size == 0) {
            /* nothing remains */
            if (using_internal) {
                /* Clear input_buffer window */
                self->in_begin = 0;
                self->in_end = 0;
            }
            /* If we are not at EOF, then we definitely need more input
               to produce more output. */
            if (!self->eof) {
                self->needs_input = 1;
            }
        } else {
            /* some input remains after this call */
            self->needs_input = 0;
            if (!using_internal) {
                /* Ensure internal buffer has enough capacity */
                if (self->input_buffer_size < in_size) {
                    char *tmp = PyMem_Realloc(self->input_buffer, in_size);
                    if (!tmp) {
                        Py_CLEAR(result);
                        PyErr_NoMemory();
                        return NULL;
                    }
                    self->input_buffer = tmp;
                    self->input_buffer_size = in_size;
                }
                memcpy(self->input_buffer, in_ptr, in_size);
                self->in_begin = 0;
                self->in_end = in_size;
            } else {
                /* using internal: advance window by already consumed amount */
                /* in_ptr/in_size already reflect the remaining range */
                self->in_begin = (size_t)(in_ptr - (const Byte*)self->input_buffer);
                self->in_end = self->in_begin + in_size;
            }
        }
    }

    if (result == NULL && !PyErr_Occurred())
        PyErr_SetString(PyExc_RuntimeError, "decode failed");
    return result;
}

static PyMethodDef Ppmd7tDecoder_methods[] = {
    {"decode", (PyCFunction)Ppmd7tDecoder_decode, METH_VARARGS|METH_KEYWORDS, Ppmd7tDecoder_decode_doc},
    {0, 0}
};

PyDoc_STRVAR(Ppmd7tDecoder_eof__doc, "True if the end-of-stream marker has been reached.");
PyDoc_STRVAR(Ppmd7tDecoder_unused_data__doc, "Data found after the end of the compressed stream.");
PyDoc_STRVAR(Ppmd7tDecoder_needs_input_doc, "True if more input is needed before more decompressed data can be produced.");

static PyMemberDef Ppmd7tDecoder_members[] = {
    {"eof", T_BOOL, offsetof(Ppmd7tDecoder, eof), READONLY, Ppmd7tDecoder_eof__doc},
    {"needs_input", T_BOOL, offsetof(Ppmd7tDecoder, needs_input), READONLY, Ppmd7tDecoder_needs_input_doc},
    {0}
};

static PyGetSetDef Ppmd7tDecoder_getset[] = {
    {"unused_data", (getter)Ppmd7t_unused_data_get, NULL, Ppmd7tDecoder_unused_data__doc},
    {0}
};

static PyType_Slot Ppmd7tDecoder_slots[] = {
    {Py_tp_new, Ppmd7tDecoder_new},
    {Py_tp_dealloc, Ppmd7tDecoder_dealloc},
    {Py_tp_init, Ppmd7tDecoder_init},
    {Py_tp_methods, Ppmd7tDecoder_methods},
    {Py_tp_members, Ppmd7tDecoder_members},
    {Py_tp_getset, Ppmd7tDecoder_getset},
    {Py_tp_doc, (char *)Ppmd7tDecoder_doc},
    {0, 0}
};

static PyType_Spec Ppmd7tDecoder_type_spec = {
    .name = "_ppmd.Ppmd7tDecoder",
    .basicsize = sizeof(Ppmd7tDecoder),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .slots = Ppmd7tDecoder_slots,
};

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

    /* Unused data */
    PyObject *unused_data;

    /* 0 if decompressor has (or may has) unconsumed input data, 0 or 1. */
    char needs_input;

    /* 1 when end mark observed */
    char eof;

    /* Output Buffer */
    BlocksOutputBuffer *blocksOutputBuffer;

    /* __init__ has been called, 0 or 1. */
    char inited;
    /* decode has been called with some data*/
    char inited2;
} Ppmd8Decoder;

typedef struct {
    PyTypeObject *Ppmd7Encoder_type;
    PyTypeObject *Ppmd7Decoder_type;
    PyTypeObject *Ppmd7tDecoder_type;
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

/* -----------------------
     Ppmd7Decoder code
   ------------------------ */
static PyObject *
Ppmd7Decoder_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Ppmd7Decoder *self;
    self = (Ppmd7Decoder*)type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }
    assert(self->inited == 0);
    assert(self->inited2 == 0);

    /* Thread lock */
    self->lock = PyThread_allocate_lock();
    if (self->lock == NULL) {
        goto error;
    }
    return (PyObject*)self;

error:
    Py_XDECREF(self);
    return PyErr_NoMemory();
}

static void
Ppmd7Decoder_dealloc(Ppmd7Decoder *self)
{
    if (self->lock) {
        PyThread_free_lock(self->lock);
    }
    if (self->cPpmd7 != NULL) {
        if (self->rangeDec != NULL) {
            BufferReader *bufferReader = (BufferReader *) self->rangeDec->Stream;
            Ppmd7T_Free(self->cPpmd7, bufferReader->t, &allocator);
            Ppmd7_Free(self->cPpmd7, &allocator);
            if (bufferReader != NULL) {
                PyMem_Free(bufferReader->inBuffer);
                PyMem_Free(bufferReader->t->out);
                PyMem_Free(bufferReader->t);
                PyMem_Free(bufferReader);
            }
            PyMem_Free(self->blocksOutputBuffer);
            PyMem_Free(self->rangeDec);
        }
        PyMem_Free(self->cPpmd7);
    }
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
    BlocksOutputBuffer *blocksOutputBuffer;
    BufferReader *bufferReader;
    InBuffer *in;
    OutBuffer *out;
    ppmd_info *threadInfo;

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
        clamp_max_order(&maximum_order, PPMD7_MAX_ORDER);
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

    bufferReader = PyMem_Malloc(sizeof(BufferReader));
    if (bufferReader == NULL) {
        PyErr_NoMemory();
        goto error;
    }
    blocksOutputBuffer = PyMem_Malloc(sizeof(BlocksOutputBuffer));
    if (blocksOutputBuffer == NULL) {
        PyMem_Free(bufferReader);
        PyErr_NoMemory();
        goto error;
    }
    in = PyMem_Malloc(sizeof(InBuffer));
    if (in == NULL) {
        PyMem_Free(bufferReader);
        PyMem_Free(blocksOutputBuffer);
        PyErr_NoMemory();
        goto error;
    }
    out = PyMem_Malloc(sizeof(OutBuffer));
    if (out == NULL) {
        PyMem_Free(in);
        PyMem_Free(blocksOutputBuffer);
        PyMem_Free(bufferReader);
        PyErr_NoMemory();
        goto error;
    }
    threadInfo = PyMem_Malloc(sizeof(ppmd_info));
    if (threadInfo == NULL) {
        PyMem_Free(out);
        PyMem_Free(in);
        PyMem_Free(blocksOutputBuffer);
        PyMem_Free(bufferReader);
        PyErr_NoMemory();
        goto error;
    }
    if ((self->cPpmd7 =  PyMem_Malloc(sizeof(CPpmd7))) != NULL) {
        Ppmd7_Construct(self->cPpmd7);
        if (Ppmd7_Alloc(self->cPpmd7, (UInt32)memory_size, &allocator)) {
            if (Ppmd_thread_decode_init(threadInfo, &allocator)) {
                Ppmd7_Init(self->cPpmd7, (unsigned int) maximum_order);
                if ((self->rangeDec = PyMem_Malloc(sizeof(CPpmd7z_RangeDec))) != NULL) {
                    bufferReader->Read = (Byte (*)(void *)) Ppmd_thread_Reader;
                    bufferReader->inBuffer = in;
                    bufferReader->t = threadInfo;
                    self->rangeDec->Stream = (IByteIn *) bufferReader;
                    threadInfo->in = in;
                    threadInfo->out = out;
                    self->eof = False;
                    self->needs_input = True;
                    self->blocksOutputBuffer = blocksOutputBuffer;
                    goto success;
                }
            }
            Ppmd7_Free(self->cPpmd7, &allocator);;
        }
        PyMem_Free(self->cPpmd7);
        PyMem_Free(out);
        PyMem_Free(in);
        PyMem_Free(blocksOutputBuffer);
        PyMem_Free(bufferReader);
        PyMem_Free(threadInfo);
        PyErr_NoMemory();
}

error:
    return -1;

success:
    return 0;
}

static PyObject *
Ppmd7_unused_data_get(Ppmd7Decoder *self, void *Py_UNUSED(ignored))
{
    PyObject *ret;

    /* Thread-safe code */
    ACQUIRE_LOCK(self);

    if (!self->eof) {
        ret = PyBytes_FromStringAndSize(NULL, 0);
    } else {
        if (self->unused_data == NULL) {
            self->unused_data = PyBytes_FromStringAndSize(
                    self->input_buffer + self->in_begin,
                    self->in_end - self->in_begin);
            ret = self->unused_data;
            Py_XINCREF(ret);
        } else {
            ret = self->unused_data;
            Py_INCREF(ret);
        }
    }

    RELEASE_LOCK(self);
    return ret;
}

PyDoc_STRVAR(Ppmd7Decoder_decode_doc, "decode()\n"
             "----\n"
             "A PPMd compression decode.");

static PyObject *
Ppmd7Decoder_decode(Ppmd7Decoder *self,  PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"data", "length", NULL};
    Py_buffer data;
    int length;
    PyObject *ret = NULL;
    char use_input_buffer;
    ppmd_info *threadInfo;

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

    BufferReader *reader = (BufferReader *) self->rangeDec->Stream;
    InBuffer *in = reader->inBuffer;
    threadInfo = reader->t;
    OutBuffer *out = threadInfo->out;

    /* Prepare input buffer w/wo unconsumed data */
    if (self->in_begin == self->in_end) {
        /* No unconsumed data */
        use_input_buffer = 0;

        in->src = data.buf;
        in->size = data.len;
        in->pos = 0;
    } else if (data.len == 0) {
        /* Has unconsumed data, fast path for b'' */
        assert(self->in_begin < self->in_end);

        use_input_buffer = 1;

        in->src = self->input_buffer + self->in_begin;
        in->size = self->in_end - self->in_begin;
        in->pos = 0;
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
        in->src = self->input_buffer + self->in_begin;
        in->size = used_now + data.len;
        in->pos = 0;
    }
    assert(in->pos == 0);

    if (OutputBuffer_InitAndGrow(self->blocksOutputBuffer, out, length) < 0) {
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
        }
        self->inited2++;
    }

    int result;
    int remains = length >= 0 ? length : INT_MAX;
    while (True) {
        Py_BEGIN_ALLOW_THREADS
        result = Ppmd7T_decode(self->cPpmd7, self->rangeDec, out, remains, threadInfo);
        Py_END_ALLOW_THREADS
        if (result < 0) {
            break; // error or eof
        }
        remains -= result;
        if (result == 0 || remains == 0) {
            break;
        }
        if (out->pos == out->size) {
            if (OutputBuffer_Grow(self->blocksOutputBuffer, out) < 0) {
                PyErr_SetString(PyExc_ValueError, "No Memory.");
                goto error;
            }
        }
    }
    if (result == 0) {
        self->needs_input = True;
    }
    if (result == -1) {
        self->eof = True;
        self->needs_input = False;
    }
    if (result == -2) {
        PyErr_SetString(PyExc_ValueError, "L1595: Corrupted input data.");
        goto error;
    }

    ret = OutputBuffer_Finish(self->blocksOutputBuffer, out);
    if (Ppmd7z_RangeDec_IsFinishedOK(self->rangeDec)) {
        self->eof = True;
    }

    /* Unconsumed input data */
    if (in->pos == in->size) {
        if (use_input_buffer) {
            /* Clear input_buffer */
            self->in_begin = 0;
            self->in_end = 0;
        }
        if (self->eof) {
            self->needs_input = False;
        } else {
            self->needs_input = True;
        }
    } else {
        const size_t data_size = in->size - in->pos;
        self->needs_input = False;
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
            memcpy(self->input_buffer, (char*)in->src + in->pos, data_size);
            self->in_begin = 0;
            self->in_end = data_size;
        } else {
            /* Use input buffer */
            self->in_begin += in->pos;
        }
    }
    goto success;

error:
    /* Reset variables */
    self->eof = True;
    self->needs_input = False;
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
        {"__reduce__", (PyCFunction)reduce_cannot_pickle,
                     METH_NOARGS, reduce_cannot_pickle_doc},
        {NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(Ppmd7Decoder_eof__doc, "True if the end-of-stream marker has been reached.");
PyDoc_STRVAR(Ppmd7Decoder_unused_data__doc, "Data found after the end of the compressed stream.");
PyDoc_STRVAR(Ppmd7Decoder_needs_input_doc, "True if more input is needed before more decompressed data can be produced.");

static PyMemberDef Ppmd7Decoder_members[] = {
        {"eof", T_BOOL, offsetof(Ppmd7Decoder, eof),
                READONLY, Ppmd7Decoder_eof__doc},

        {"needs_input", T_BOOL, offsetof(Ppmd7Decoder, needs_input),
                READONLY, Ppmd7Decoder_needs_input_doc},

        {NULL}
};

static PyGetSetDef Ppmd7Decoder_getset[] = {
        {"unused_data", (getter)Ppmd7_unused_data_get, NULL,
                Ppmd7Decoder_unused_data__doc},
        {NULL},
};

static PyType_Slot Ppmd7Decoder_slots[] = {
    {Py_tp_new, Ppmd7Decoder_new},
    {Py_tp_dealloc, Ppmd7Decoder_dealloc},
    {Py_tp_init, Ppmd7Decoder_init},
    {Py_tp_methods, Ppmd7Decoder_methods},
    {Py_tp_members, Ppmd7Decoder_members},
    {Py_tp_getset, Ppmd7Decoder_getset},
    {Py_tp_doc, (char *)Ppmd7Decoder_doc},
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
    self = (Ppmd7Encoder*)type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }
    assert(self->inited == 0);
    /* Thread lock */
    if ((self->lock = PyThread_allocate_lock()) == NULL) {
        goto error;
    }
    return (PyObject*)self;

error:
    Py_XDECREF(self);
    return PyErr_NoMemory();
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
        clamp_max_order(&maximum_order, PPMD7_MAX_ORDER);
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
        PyErr_NoMemory();
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
    OutBuffer out;
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

    writer.Write = (void (*)(void *, Byte)) Writer;
    writer.outBuffer = &out;
    self->rangeEnc->Stream = (IByteOut *) &writer;

    Bool result = True;
    for (UInt32 i = 0; i < data.len; i++){
        Py_BEGIN_ALLOW_THREADS
        Ppmd7_EncodeSymbol(self->cPpmd7, self->rangeEnc, *((Byte *)data.buf + i));
        Py_END_ALLOW_THREADS
        if (out.size == out.pos) {
            if (OutputBuffer_Grow(&buffer, &out) < 0) {
                PyErr_SetString(PyExc_ValueError, "No memory.");
                result = False;
                break;
            } else {
                writer.outBuffer = &out;
            }
        }
    }
    if (!result)
        goto error;

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
    OutBuffer out;
    BlocksOutputBuffer buffer = { 0 };
    BufferWriter writer;
    static char *kwlist[] = {"endmark", NULL};
    Bool endmark = False;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "|p:Ppmd7Encoder.flush", kwlist,
                                     &endmark)) {
        goto error;
    }

    ACQUIRE_LOCK(self);
    if (self->flushed) {
        PyErr_SetString(PyExc_ValueError, flush_twice_msg);
        goto error;
    }

    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        PyErr_SetString(PyExc_ValueError, "No memory.");
        goto error;
    }

    writer.Write = (void (*)(void *, Byte)) Writer;
    writer.outBuffer = &out;
    rc->Stream = (IByteOut *) &writer;

    if (endmark) {
        Ppmd7_EncodeSymbol(self->cPpmd7, rc, -1);
    }
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
        {Py_tp_doc, (char *)Ppmd7Encoder_doc},
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
        return NULL;
    }
    assert(self->inited == 0);
    assert(self->inited2 == 0);

    /* Thread lock */
    self->lock = PyThread_allocate_lock();
    if (self->lock == NULL) {
        goto error;
    }
    return (PyObject*)self;

error:
    Py_XDECREF(self);
    return PyErr_NoMemory();
}

static void
Ppmd8Decoder_dealloc(Ppmd8Decoder *self) {
    if (self->lock) {
        PyThread_free_lock(self->lock);
    }
    if (self->cPpmd8 != NULL) {
        BufferReader *bufferReader = (BufferReader *) self->cPpmd8->Stream.In;
        Ppmd8T_Free(self->cPpmd8, bufferReader->t, &allocator);
        Ppmd8_Free(self->cPpmd8, &allocator);
        if (bufferReader != NULL) {
            PyMem_Free(bufferReader->inBuffer);
            PyMem_Free(bufferReader->t->out);
            PyMem_Free(bufferReader->t);
            PyMem_Free(bufferReader);
        }
        PyMem_Free(self->blocksOutputBuffer);
        PyMem_Free(self->cPpmd8);
    }
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free((PyObject*)self);
    Py_DECREF(tp);
}

PyDoc_STRVAR(Ppmd8Decoder_doc, "A PPMd compression algorithm decoder.\n\n"
                                 "Ppmd8Decoder.__init__(self, max_order, mem_size, restore_method=0)\n"
                                 "----\n"
                                 "Initialize a Ppmd8Decoder object.\n\n"
                                 "Arguments\n"
                                 "max_order: max order for the PPM modelling ranging from 2 to 64,\n"
                                 "           higher values produce better compression ratios but are slower.\n"
                                 "           Default is 6.\n"
                                 "mem_size:  max memory size in bytes the compressor is able to use, bigger values improve compression,\n"
                                 "           raging from 10kB to physical memory size.\n"
                                 "           Default size is 16MB.\n"
                                 "restore_method: restore method, 0=restart, 1=cutoff.\n"
                                 );

static int
Ppmd8Decoder_init(Ppmd8Decoder *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"max_order", "mem_size", "restore_method", NULL};
    PyObject *max_order = Py_None;
    PyObject *mem_size = Py_None;
    int restore_method = PPMD8_RESTORE_METHOD_RESTART;
    BlocksOutputBuffer *blocksOutputBuffer;
    BufferReader *bufferReader;
    InBuffer *in;
    OutBuffer *out;
    ppmd_info *threadInfo;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "OO|i:Ppmd8Decoder.__init__", kwlist,
                                     &max_order, &mem_size, &restore_method)) {
        return -1;
    }

    /* Only called once */
    if (self->inited) {
        PyErr_SetString(PyExc_RuntimeError, init_twice_msg);
        goto error;
    }
    self->inited = 1;
    self->needs_input = 1;

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
        clamp_max_order(&maximum_order, PPMD8_MAX_ORDER);
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

    bufferReader = PyMem_Malloc(sizeof(BufferReader));
    if (bufferReader == NULL) {
        PyErr_NoMemory();
        goto error;
    }
    blocksOutputBuffer = PyMem_Malloc(sizeof(BlocksOutputBuffer));
    if (blocksOutputBuffer == NULL) {
        PyMem_Free(bufferReader);
        PyErr_NoMemory();
        goto error;
    }
    in = PyMem_Malloc(sizeof(InBuffer));
    if (in == NULL) {
        PyMem_Free(bufferReader);
        PyMem_Free(blocksOutputBuffer);
        PyErr_NoMemory();
        goto error;
    }
    out = PyMem_Malloc(sizeof(OutBuffer));
    if (out == NULL) {
        PyMem_Free(in);
        PyMem_Free(blocksOutputBuffer);
        PyMem_Free(bufferReader);
        PyErr_NoMemory();
        goto error;
    }
    threadInfo = PyMem_Malloc(sizeof(ppmd_info));
    if (threadInfo == NULL) {
        PyMem_Free(out);
        PyMem_Free(in);
        PyMem_Free(blocksOutputBuffer);
        PyMem_Free(bufferReader);
        PyErr_NoMemory();
        goto error;
    }
    if ((self->cPpmd8 = PyMem_Malloc(sizeof(CPpmd8))) != NULL) {
        Ppmd8_Construct(self->cPpmd8);
        if (Ppmd8_Alloc(self->cPpmd8, memory_size ,&allocator)) {
            if (Ppmd_thread_decode_init(threadInfo, &allocator)) {
                Ppmd8_Init(self->cPpmd8, maximum_order, restore_method);
                bufferReader->Read = (Byte (*)(void *)) Ppmd_thread_Reader;
                bufferReader->inBuffer = in;
                bufferReader->t = threadInfo;
                self->cPpmd8->Stream.In = (IByteIn *) bufferReader;
                threadInfo->cPpmd = (void *) (self->cPpmd8);
                threadInfo->in = in;
                threadInfo->out = out;
                self->blocksOutputBuffer = blocksOutputBuffer;
                goto success;
            }
            Ppmd8_Free(self->cPpmd8, &allocator);
        }
        PyMem_Free(self->cPpmd8);
        PyMem_Free(out);
        PyMem_Free(in);
        PyMem_Free(blocksOutputBuffer);
        PyMem_Free(bufferReader);
        PyErr_NoMemory();
    }

error:
    return -1;

success:
    return 0;
}

static PyObject *
unused_data_get(Ppmd8Decoder *self, void *Py_UNUSED(ignored))
{
    PyObject *ret;

    /* Thread-safe code */
    ACQUIRE_LOCK(self);

    if (!self->eof) {
        ret = PyBytes_FromStringAndSize(NULL, 0);
    } else {
        if (self->unused_data == NULL) {
            self->unused_data = PyBytes_FromStringAndSize(
                                    self->input_buffer + self->in_begin,
                                    self->in_end - self->in_begin);
            ret = self->unused_data;
            Py_XINCREF(ret);
        } else {
            ret = self->unused_data;
            Py_INCREF(ret);
        }
    }

    RELEASE_LOCK(self);
    return ret;
}

PyDoc_STRVAR(Ppmd8Decoder_decode_doc, "decode()\n"
             "----\n"
             "A PPMd compression decode.");

static PyObject *
Ppmd8Decoder_decode(Ppmd8Decoder *self,  PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"data", "length", NULL};
    Py_buffer data;
    int length = -1;
    PyObject *ret = NULL;
    char use_input_buffer;
    ppmd_info *threadInfo;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "y*|i:Ppmd8Decoder.decode", kwlist,
                                     &data, &length)) {
        return NULL;
    }

    if (self->inited2 == 0 && data.len < 5) {
       PyErr_SetString(PyExc_ValueError,
                       "Not enough data for starting decompression.");
       return NULL;
    }

    ACQUIRE_LOCK(self);

    BufferReader *bufferReader = (BufferReader *) self->cPpmd8->Stream.In;
    InBuffer *in = bufferReader->inBuffer;
    threadInfo = bufferReader->t;
    OutBuffer *out = threadInfo->out;

    /* Prepare input buffer w/wo unconsumed data */
    if (self->in_begin == self->in_end) {
        /* No unconsumed data */
        use_input_buffer = 0;

        in->src = data.buf;
        in->size = data.len;
        in->pos = 0;
    } else if (data.len == 0) {
        /* Has unconsumed data, fast path for b'' */
        assert(self->in_begin < self->in_end);

        use_input_buffer = 1;

        in->src = self->input_buffer + self->in_begin;
        in->size = self->in_end - self->in_begin;
        in->pos = 0;
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
        in->src = self->input_buffer + self->in_begin;
        in->size = used_now + data.len;
        in->pos = 0;
    }
    assert(in->pos == 0);

    if (OutputBuffer_InitAndGrow(self->blocksOutputBuffer, out, length) < 0) {
        PyErr_SetString(PyExc_ValueError, "L1551: No Memory.");
        RELEASE_LOCK(self);
        return NULL;
    }

    if (self->inited2 == 0) {
        // first time initialized.
        assert(use_input_buffer == 0);
        if (!Ppmd8_RangeDec_Init(self->cPpmd8)) {
            RELEASE_LOCK(self);
            return NULL;
        }
        self->inited2++;
    }

    int result;
    int remains = length >= 0 ? length : INT_MAX;
    while (True) {
        Py_BEGIN_ALLOW_THREADS
        result = Ppmd8T_decode(self->cPpmd8, out, remains, threadInfo);
        Py_END_ALLOW_THREADS
        if (result < 0) {
            break; // error or eof
        }
        if (result == 0) {
            break;  // no output data
        }
        if ((remains -= result) == 0) {
             break;  // filled expected
        }
        if (out->pos == out->size) {
            if (OutputBuffer_Grow(self->blocksOutputBuffer, out) < 0) {
                PyErr_SetString(PyExc_ValueError, "L1586: Unknown status");
                goto error;
            }
        }
    }
    if (result == 0) {
        self->needs_input = True;
    }
    if (result == -1) {
        self->eof = True;
        self->needs_input = False;
    }
    if (result == -2) {
        PyErr_SetString(PyExc_ValueError, "L1595: Corrupted input data.");
        goto error;
    }

    ret = OutputBuffer_Finish(self->blocksOutputBuffer, out);

    /* Unconsumed input data */
    if (in->pos == in->size) {
        if (use_input_buffer) {
            /* Clear input_buffer */
            self->in_begin = 0;
            self->in_end = 0;
        }
    } else {
        const size_t data_size = in->size - in->pos;
        self->needs_input = False;
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
            memcpy(self->input_buffer, (char*)in->src + in->pos, data_size);
            self->in_begin = 0;
            self->in_end = data_size;
        } else {
            /* Use input buffer */
            self->in_begin += in->pos;
        }
    }
    goto success;

error:
    /* Reset variables */
    self->eof = True;
    self->needs_input = False;
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
        {"__reduce__", (PyCFunction)reduce_cannot_pickle,
                     METH_NOARGS, reduce_cannot_pickle_doc},
        {NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(Ppmd8Decoder_eof__doc, "True if the end-of-stream marker has been reached.");
PyDoc_STRVAR(Ppmd8Decoder_unused_data__doc, "Data found after the end of the compressed stream.");
PyDoc_STRVAR(Ppmd8Decoder_needs_input_doc, "True if more input is needed before more decompressed data can be produced.");

static PyMemberDef Ppmd8Decoder_members[] = {
    {"eof", T_BOOL, offsetof(Ppmd8Decoder, eof),
     READONLY, Ppmd8Decoder_eof__doc},

    {"needs_input", T_BOOL, offsetof(Ppmd8Decoder, needs_input),
     READONLY, Ppmd8Decoder_needs_input_doc},

    {NULL}
};

static PyGetSetDef Ppmd8Decoder_getset[] = {
    {"unused_data", (getter)unused_data_get, NULL,
      Ppmd8Decoder_unused_data__doc},
    {NULL},
};

static PyType_Slot Ppmd8Decoder_slots[] = {
    {Py_tp_new, Ppmd8Decoder_new},
    {Py_tp_dealloc, Ppmd8Decoder_dealloc},
    {Py_tp_init, Ppmd8Decoder_init},
    {Py_tp_methods, Ppmd8Decoder_methods},
    {Py_tp_members, Ppmd8Decoder_members},
    {Py_tp_getset, Ppmd8Decoder_getset},
    {Py_tp_doc, (char *)Ppmd8Decoder_doc},
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
    self = (Ppmd8Encoder*)type->tp_alloc(type, 0);
     if (self == NULL) {
        return NULL;
     }
    assert(self->inited == 0);
    /* Thread lock */
    if ((self->lock = PyThread_allocate_lock()) == NULL) {
        goto error;
    }
    return (PyObject*)self;

error:
    Py_XDECREF(self);
    return PyErr_NoMemory();
}

static void
Ppmd8Encoder_dealloc(Ppmd8Encoder *self)
{
    if (self->cPpmd8 != NULL) {
        Ppmd8_Free(self->cPpmd8, &allocator);
    }
    if (self->lock) {
        PyThread_free_lock(self->lock);
    }
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free((PyObject*)self);
    Py_DECREF(tp);
}

PyDoc_STRVAR(Ppmd8Encoder_doc, "A PPMd compression algorithm.\n\n"
                                 "Ppmd8Encoder.__init__(self, max_order, mem_size, restore_method=0)\n"
                                 "----\n"
                                 "Initialize a Ppmd8Encoder object.\n\n"
                                 "Arguments\n"
                                 "max_order: max order for the PPM modelling ranging from 2 to 64, higher values produce better compression ratios but are slower.\n"
                                 "           Default is 6.\n"
                                 "mem_size:  max memory size in bytes the compressor is able to use, bigger values improve compression,\n"
                                 "           raging from 10kB to physical memory size.\n"
                                 "           Default size is 16MB.\n"
                                 "restore_method: restore method, 0=restart, 1=cutoff.\n"
                                 );

static int
Ppmd8Encoder_init(Ppmd8Encoder *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"max_order", "mem_size", "restore_method", NULL};
    PyObject *max_order = Py_None;
    PyObject *mem_size = Py_None;
    int restore_method = PPMD8_RESTORE_METHOD_RESTART;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "OO|i:Ppmd8Encoder.__init__", kwlist,
                                     &max_order, &mem_size, &restore_method)) {
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
        clamp_max_order(&maximum_order, PPMD8_MAX_ORDER);
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
            Ppmd8_Init(self->cPpmd8, (unsigned int)maximum_order, restore_method);
            goto success;
        }
        PyMem_Free(self->cPpmd8);
        PyErr_NoMemory();
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
    OutBuffer out;
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

    writer.Write = (void (*)(void *, Byte)) Writer;
    writer.outBuffer = &out;
    self->cPpmd8->Stream.Out = (IByteOut *)&writer;

    Bool result = True;
    for (UInt32 i = 0; i < data.len; i++){
        Py_BEGIN_ALLOW_THREADS
        Ppmd8_EncodeSymbol(self->cPpmd8, *((Byte *)data.buf + i));
        Py_END_ALLOW_THREADS
        if (out.size == out.pos) {
            if (OutputBuffer_Grow(&buffer, &out) < 0) {
                PyErr_SetString(PyExc_ValueError, "No memory.");
                result = False;
                break;
            } else {
                writer.outBuffer = &out;
            }
        }
    }
    if (!result)
        goto error;

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
    OutBuffer out;
    BlocksOutputBuffer buffer = { 0 };
    BufferWriter writer;
    static char *kwlist[] = {"endmark", NULL};
    Bool endmark = True;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "|p:Ppmd8Encoder.flush", kwlist,
                                     &endmark)) {
        goto error;
    }

    ACQUIRE_LOCK(self);
    if (self->flushed) {
        PyErr_SetString(PyExc_ValueError, flush_twice_msg);
        goto error;
    }

    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        PyErr_SetString(PyExc_ValueError, "No memory.");
        goto error;
    }

    writer.Write = (void (*)(void *, Byte)) Writer;
    writer.outBuffer = &out;
    self->cPpmd8->Stream.Out = (IByteOut *) &writer;
    if (endmark) {
        Ppmd8_EncodeSymbol(self->cPpmd8, -1);
    }
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
        {Py_tp_doc, (char *)Ppmd8Encoder_doc},
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
    Py_VISIT(static_state.Ppmd7tDecoder_type);
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
    Py_CLEAR(static_state.Ppmd7tDecoder_type);
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
    PyModule_AddIntConstant(module, "PPMD8_RESTORE_METHOD_RESTART", 0);
    PyModule_AddIntConstant(module, "PPMD8_RESTORE_METHOD_CUT_OFF", 1);
    // #ifdef PPMD8_FREEZE_SUPPORT
    // PyModule_AddIntConstant(module, "PPMD8_RESTORE_METHOD_FREEZE", 2);
    // #endif

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
                           "Ppmd7tDecoder",
                           &Ppmd7tDecoder_type_spec,
                           &static_state.Ppmd7tDecoder_type) < 0) {
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

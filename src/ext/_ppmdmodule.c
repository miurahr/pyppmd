/* pyppmd module for Python 3.6+ */
#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "Ppmd7.h"

typedef struct {
    /* Inherits from IByteOut */
    void (*Write)(void *p, Byte b);
    PPMD_outBuffer *buf;
} BufferWriter;

typedef struct {
    /* Inherits from IByteIn */
    Byte (*Read)(void *p);
    PPMD_inBuffer *buf;
} BufferReader;



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


static void Write(void *p, Byte b) {
    BufferWriter *bwriter = (BufferWriter *) p;
    PPMD_outBuffer *obuf = bwriter->buf;
    size_t rest = obuf->size - obuf->pos;
    if (rest > 0) {
        char *buf = obuf->dst;
        buf += obuf->pos;
        obuf->pos++;
        *buf = b;
    }
}

Byte src_read(void *p) {
    BufferReader *breader = (BufferReader *) p;
    PPMD_inBuffer *ibuf = breader->buf;
    size_t rest = ibuf->size - ibuf->pos;
    if (rest > 0) {
        const char *buf = ibuf->src;
        return buf[ibuf->pos++];
    }
    return (Byte)-1;
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
} Ppmd7Encoder;

typedef struct {
    PyObject_HEAD

    /* Unconsumed input data */
    char *input_buffer;
    size_t input_buffer_size;
    size_t in_begin, in_end;

    /* Thread lock for decompressing */
    PyThread_type_lock lock;

    /* Unused data */
    PyObject *unused_data;

    /* 0 if decompressor has (or may has) unconsumed input data, 0 or 1. */
    char needs_input;

    char eof;

    /* Ppmd7 context */
    CPpmd7 *cPpmd7;

    /* __init__ has been called, 0 or 1. */
    char inited;
} Ppmd7Decoder;

typedef struct {
    PyTypeObject *Ppmd7Encoder_type;
    PyTypeObject *Ppmd7Decoder_type;
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

/* Force inlining */
#if defined(__GNUC__) || defined(__ICCARM__)
#  define FORCE_INLINE static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#  define FORCE_INLINE static inline __forceinline
#else
#  define FORCE_INLINE static inline
#endif

/* Force no inlining */
#ifdef _MSC_VER
#  define FORCE_NO_INLINE static __declspec(noinline)
#else
#  if defined(__GNUC__) || defined(__ICCARM__)
#    define FORCE_NO_INLINE static __attribute__((__noinline__))
#  else
#    define FORCE_NO_INLINE static
#  endif
#endif

static const char init_twice_msg[] = "__init__ method is called twice.";

typedef enum {
    ERR_DECOMPRESS,
    ERR_COMPRESS,
    ERR_SET_LEVEL,
    ERR_SET_MEM_SIZE,
} error_type;

/* -----------------------
     Ppmd7Encoder code
   ----------------------- */
static PyObject *
Ppmd7Encoder_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Ppmd7Encoder *self;
    self = (Ppmd7Encoder*)type->tp_alloc(type, 0);
    if (self == NULL) {
        goto error;
    }
    assert(self->inited == 0);

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
                                     "|OO:Ppmd7Encoder.__init__", kwlist,
                                     &max_order, &mem_size)) {
        return -1;
    }

    /* Only called once */
    if (self->inited) {
        PyErr_SetString(PyExc_RuntimeError, init_twice_msg);
        return -1;
    }
    self->inited = 1;

    unsigned long maximum_order = 6;

    /* Set Compression level */
    if (max_order != Py_None) {
        if (PyLong_Check(max_order)) {
            maximum_order = PyLong_AsUnsignedLong(max_order);
            if (maximum_order == (unsigned long)-1 && PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError,
                                "Max_order should be signed int value ranging from 2 to 16.");
                return -1;
            }
        }
        /* Clamp compression level */
        clamp_max_order(&maximum_order);
    } else {
        maximum_order = 6;
    }

    unsigned long memory_size = 16 << 20;

    /* Check memory size and alloc buffer memory */
    if (mem_size != Py_None) {
        if (PyLong_Check(mem_size)) {
            memory_size = PyLong_AsUnsignedLong(mem_size);
            if (memory_size == (unsigned long)-1 && PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError,
                                "Memory size should be unsigned long value.");
                return -1;
            }
        }
        /* Clamp memory size */
        clamp_memory_size(&memory_size);
    } else {
        memory_size = 16 << 20;
    }

    self->cPpmd7 =  PyMem_Malloc(sizeof(CPpmd7));
    Ppmd7_Construct(self->cPpmd7);
    Ppmd7_Alloc(self->cPpmd7, memory_size, &allocator);
    Ppmd7_Init(self->cPpmd7, (int)maximum_order);
    CPpmd7z_RangeEnc *rc = PyMem_Malloc(sizeof(CPpmd7z_RangeEnc));
    Ppmd7z_RangeEnc_Init(rc);
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
    CPpmd7 *ppmd = self->cPpmd7;
    CPpmd7z_RangeEnc *rc = self->rangeEnc;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "y*|i:Ppmd7Encoder.encode", kwlist,
                                     &data)) {
        return NULL;
    }

    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        goto error;
    }

    BufferWriter *writer = PyMem_Malloc(sizeof(BufferWriter));
    writer->Write = Write;
    writer->buf = &out;
    rc->Stream = (IByteOut *) writer;

    ACQUIRE_LOCK(self);
    for (UInt32 i = 0; i < data.len; i++){
        Ppmd7_EncodeSymbol(ppmd, rc, *(char *)(data.buf + i));
    }
    RELEASE_LOCK(self);

    ret = OutputBuffer_Finish(&buffer, &out);
    PyMem_Free(writer);
    return ret;
error:
    OutputBuffer_OnError(&buffer);
    return NULL;
}

PyDoc_STRVAR(Ppmd7Encoder_flush_doc, "flush()\n"
"----\n"
"Flush any remaining data in internal buffer.");

static PyObject *
Ppmd7Encoder_flush(Ppmd7Encoder *self,  PyObject *args, PyObject *kwargs)
{
    PyObject *ret;
    CPpmd7z_RangeEnc *rc = self->rangeEnc;
    PPMD_outBuffer out;
    BlocksOutputBuffer buffer;
    if (OutputBuffer_InitAndGrow(&buffer, &out, -1) < 0) {
        goto error;
    }

    BufferWriter *writer = PyMem_Malloc(sizeof(BufferWriter));
    writer->Write = Write;
    writer->buf = &out;
    rc->Stream = (IByteOut *) writer;

    ACQUIRE_LOCK(self);
    Ppmd7z_RangeEnc_FlushData(rc);
    RELEASE_LOCK(self);
    ret = OutputBuffer_Finish(&buffer, &out);
    PyMem_Free(writer);
    return ret;
error:
    OutputBuffer_OnError(&buffer);
    return NULL;
}

static PyMethodDef _ppmd_methods[] = {
    {NULL}
};


/* --------------------
     Initialize code
   -------------------- */
static int
_ppmd_traverse(PyObject *module, visitproc visit, void *arg)
{
    Py_VISIT(static_state.PpmdError);
    Py_VISIT(static_state.Ppmd7Encoder_type);
    return 0;
}

static int
_ppmd_clear(PyObject *module)
{
    Py_CLEAR(static_state.PpmdError);
    Py_CLEAR(static_state.Ppmd7Encoder_type);
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
    return module;

error:
     _ppmd_clear(NULL);
     Py_XDECREF(module);

     return NULL;
}
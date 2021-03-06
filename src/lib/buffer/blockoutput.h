//
// Created by miurahr on 22/05/18.
//

#ifndef PYPPMD_BLOCKOUTPUT_H
#define PYPPMD_BLOCKOUTPUT_H
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
        {32 * KB, 64 * KB, 256 * KB, 1 * MB, 4 * MB, 8 * MB, 16 * MB, 16 * MB,
         32 * MB, 32 * MB, 32 * MB, 32 * MB, 64 * MB, 64 * MB, 128 * MB, 128 * MB,
         256 * MB};

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
OutputBuffer_InitAndGrow(BlocksOutputBuffer *buffer, OutBuffer *ob,
                         Py_ssize_t max_length) {
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
OutputBuffer_InitWithSize(BlocksOutputBuffer *buffer, OutBuffer *ob,
                          Py_ssize_t init_size) {
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
OutputBuffer_Grow(BlocksOutputBuffer *buffer, OutBuffer *ob) {
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
OutputBuffer_ReachedMaxLength(BlocksOutputBuffer *buffer, OutBuffer *ob) {
    /* Ensure (data size == allocated size) */
    assert(ob->pos == ob->size);

    return buffer->allocated == buffer->max_length;
}


/* Finish the buffer.
   Return a bytes object on success
   Return NULL on failure
*/
static PyObject *
OutputBuffer_Finish(BlocksOutputBuffer *buffer, OutBuffer *ob) {
    PyObject *result, *block;
    const Py_ssize_t list_len = Py_SIZE(buffer->list);

    /* Fast path for single block */
    if ((list_len == 1 && ob->pos == ob->size) ||
        (list_len == 2 && ob->pos == 0)) {
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
        for (; i < list_len - 1; i++) {
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

#endif //PYPPMD_BLOCKOUTPUT_H

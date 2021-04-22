import sys
from threading import Lock
from typing import Union

from ._cffi_ppmd import ffi, lib

__all__ = ("Ppmd7Encoder", "Ppmd7Decoder", "Ppmd8Encoder", "Ppmd8Decoder")

_PPMD7_MIN_ORDER = 2
_PPMD7_MAX_ORDER = 64

_PPMD7_MIN_MEM_SIZE = 1 << 11
_PPMD7_MAX_MEM_SIZE = 0xFFFFFFFF - 12 * 3

_BLOCK_SIZE = 16384
_allocated = []

CFFI_PYPPMD = True

_new_nonzero = ffi.new_allocator(should_clear_after_alloc=False)


class PpmdError(Exception):
    "Call to the underlying library failed."
    pass


@ffi.def_extern()
def raw_alloc(size: int) -> object:
    if size == 0:
        return ffi.NULL
    block = ffi.new("char[]", size)
    _allocated.append(block)
    return block


@ffi.def_extern()
def raw_free(o: object) -> None:
    if o in _allocated:
        _allocated.remove(o)


class _BlocksOutputBuffer:
    KB = 1024
    MB = 1024 * 1024
    BUFFER_BLOCK_SIZE = (
        32 * KB,
        64 * KB,
        256 * KB,
        1 * MB,
        4 * MB,
        8 * MB,
        16 * MB,
        16 * MB,
        32 * MB,
        32 * MB,
        32 * MB,
        32 * MB,
        64 * MB,
        64 * MB,
        128 * MB,
        128 * MB,
        256 * MB,
    )
    MEM_ERR_MSG = "Unable to allocate output buffer."

    def initAndGrow(self, out, max_length):
        # Set & check max_length
        self.max_length = max_length
        if 0 <= max_length < self.BUFFER_BLOCK_SIZE[0]:
            block_size = max_length
        else:
            block_size = self.BUFFER_BLOCK_SIZE[0]

        # The first block
        block = _new_nonzero("char[]", block_size)
        if block == ffi.NULL:
            raise MemoryError

        # Create the list
        self.list = [block]

        # Set variables
        self.allocated = block_size
        out.dst = block
        out.size = block_size
        out.pos = 0

    def initWithSize(self, out, init_size):
        # The first block
        block = _new_nonzero("char[]", init_size)
        if block == ffi.NULL:
            raise MemoryError(self.MEM_ERR_MSG)

        # Create the list
        self.list = [block]

        # Set variables
        self.allocated = init_size
        self.max_length = -1
        out.dst = block
        out.size = init_size
        out.pos = 0

    def grow(self, out):
        # Ensure no gaps in the data
        assert out.pos == out.size

        # Get block size
        list_len = len(self.list)
        if list_len < len(self.BUFFER_BLOCK_SIZE):
            block_size = self.BUFFER_BLOCK_SIZE[list_len]
        else:
            block_size = self.BUFFER_BLOCK_SIZE[-1]

        # Check max_length
        if self.max_length >= 0:
            # If (rest == 0), should not grow the buffer.
            rest = self.max_length - self.allocated
            assert rest > 0

            # block_size of the last block
            if block_size > rest:
                block_size = rest

        # Create the block
        b = _new_nonzero("char[]", block_size)
        if b == ffi.NULL:
            raise MemoryError(self.MEM_ERR_MSG)
        self.list.append(b)

        # Set variables
        self.allocated += block_size
        out.dst = b
        out.size = block_size
        out.pos = 0

    def reachedMaxLength(self, out):
        # Ensure (data size == allocated size)
        assert out.pos == out.size

        return self.allocated == self.max_length

    def finish(self, out):
        # Fast path for single block
        if (len(self.list) == 1 and out.pos == out.size) or (len(self.list) == 2 and out.pos == 0):
            return bytes(ffi.buffer(self.list[0]))

        # Final bytes object
        data_size = self.allocated - (out.size - out.pos)
        final = _new_nonzero("char[]", data_size)
        if final == ffi.NULL:
            raise MemoryError(self.MEM_ERR_MSG)

        # Memory copy
        # Blocks except the last one
        posi = 0
        for block in self.list[:-1]:
            ffi.memmove(final + posi, block, len(block))
            posi += len(block)
        # The last block
        ffi.memmove(final + posi, self.list[-1], out.pos)

        return bytes(ffi.buffer(final))


class PpmdBaseEncoder:
    def __init__(self):
        pass

    def _init_common(self):
        self.lock = Lock()
        self.closed = False
        self.flushed = False
        self.writer = ffi.new("BufferWriter *")
        self._allocator = ffi.new("ISzAlloc *")
        self._allocator.Alloc = lib.raw_alloc
        self._allocator.Free = lib.raw_free

    def _setup_inBuffer(self, data):
        # Input buffer
        in_buf = _new_nonzero("PPMD_inBuffer *")
        if in_buf == ffi.NULL:
            raise MemoryError
        in_buf.src = ffi.from_buffer(data)
        in_buf.size = len(data)
        in_buf.pos = 0
        return in_buf

    def _setup_outBuffer(self):
        # Output buffer
        out_buf = _new_nonzero("PPMD_outBuffer *")
        self.writer.outBuffer = out_buf
        if out_buf == ffi.NULL:
            raise MemoryError
        out = _BlocksOutputBuffer()

        # Initialize output buffer
        out.initAndGrow(out_buf, -1)
        return (out, out_buf)

    def encode(self, data) -> bytes:
        return b""

    def flush(self) -> bytes:
        return b""

    def _release(self):
        ffi.release(self._allocator)
        ffi.release(self.writer)

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        if not self.flushed:
            self.flush()


class PpmdBaseDecoder:
    def __init__(self):
        pass

    def _init_common(self):
        self.lock = Lock()
        self._allocator = ffi.new("ISzAlloc *")
        self._allocator.Alloc = lib.raw_alloc
        self._allocator.Free = lib.raw_free
        self.reader = ffi.new("BufferReader *")
        self._input_buffer = ffi.NULL
        self._input_buffer_size = 0
        self._in_begin = 0
        self._in_end = 0
        self.closed = False
        self.inited = False

    def _release(self):
        ffi.release(self.reader)
        ffi.release(self._allocator)

    def _setup_inBuffer(self, data):
        # Input buffer
        in_buf = _new_nonzero("PPMD_inBuffer *")
        # Prepare input buffer w/wo unconsumed data
        if self._in_begin == self._in_end:
            # No unconsumed data
            use_input_buffer = False

            in_buf.src = ffi.from_buffer(data)
            in_buf.size = len(data)
            in_buf.pos = 0
        elif len(data) == 0:
            # Has unconsumed data, fast path for b"".
            assert self._in_begin < self._in_end
            use_input_buffer = True

            in_buf.src = self._input_buffer + self._in_begin
            in_buf.size = self._in_end - self._in_begin
            in_buf.pos = 0
        else:
            # Has unconsumed data
            use_input_buffer = True

            # Unconsumed data size in input_buffer
            used_now = self._in_end - self._in_begin
            # Number of bytes we can append to input buffer
            avail_now = self._input_buffer_size - self._in_end
            # Number of bytes we can append if we move existing
            # contents to beginning of buffer
            avail_total = self._input_buffer_size - used_now

            assert used_now > 0 and avail_now >= 0 and avail_total >= 0

            if avail_total < len(data):
                new_size = used_now + len(data)
                # Allocate with new size
                tmp = _new_nonzero("char[]", new_size)
                if tmp == ffi.NULL:
                    raise MemoryError

                # Copy unconsumed data to the beginning of new buffer
                ffi.memmove(tmp, self._input_buffer + self._in_begin, used_now)

                # Switch to new buffer
                self._input_buffer = tmp
                self._input_buffer_size = new_size

                # Set begin & end position
                self._in_begin = 0
                self._in_end = used_now
            elif avail_now < len(data):
                # Move unconsumed data to the beginning
                ffi.memmove(self._input_buffer, self._input_buffer + self._in_begin, used_now)

                # Set begin & end position
                self._in_begin = 0
                self._in_end = used_now

            # Copy data to input buffer
            ffi.memmove(self._input_buffer + self._in_end, ffi.from_buffer(data), len(data))
            self._in_end += len(data)

            in_buf.src = self._input_buffer + self._in_begin
            in_buf.size = used_now + len(data)
            in_buf.pos = 0

        # Now in_buf.pos == 0

        self.reader.inBuffer = in_buf
        return in_buf, use_input_buffer

    def _setup_outBuffer(self):
        # Output buffer
        out_buf = _new_nonzero("PPMD_outBuffer *")
        if out_buf == ffi.NULL:
            raise MemoryError
        out = _BlocksOutputBuffer()

        # Initialize output buffer
        out.initAndGrow(out_buf, -1)
        return out, out_buf

    def _unconsumed_in(self, in_buf, use_input_buffer):
        # Unconsumed input data
        if in_buf.pos == in_buf.size:
            if use_input_buffer:
                # Clear input_buffer
                self._in_begin = 0
                self._in_end = 0
        elif in_buf.pos < in_buf.size:
            data_size = in_buf.size - in_buf.pos
            if not use_input_buffer:
                # Discard buffer if it's too small
                if self._input_buffer == ffi.NULL or self._input_buffer_size < data_size:
                    # Create new buffer
                    self._input_buffer = _new_nonzero("char[]", data_size)
                    if self._input_buffer == ffi.NULL:
                        self._input_buffer_size = 0
                        raise MemoryError
                    # Set buffer size
                    self._input_buffer_size = data_size

                # Copy unconsumed data
                ffi.memmove(self._input_buffer, in_buf.src + in_buf.pos, data_size)
                self._in_begin = 0
                self._in_end = data_size
            else:
                # Use input buffer
                self._in_begin += in_buf.pos
        else:
            raise PpmdError("Wrong status: input buffer overrun.")


class Ppmd7Encoder(PpmdBaseEncoder):
    def __init__(self, max_order: int, mem_size: int):
        if mem_size > sys.maxsize:
            raise ValueError("Mem_size exceed to platform limit.")
        if (_PPMD7_MIN_ORDER > max_order or max_order > _PPMD7_MAX_ORDER) or (
            _PPMD7_MIN_MEM_SIZE > mem_size or mem_size > _PPMD7_MAX_MEM_SIZE
        ):
            raise ValueError("PPMd wrong parameters.")
        self._init_common()
        self.ppmd = ffi.new("CPpmd7 *")
        self.rc = ffi.new("CPpmd7z_RangeEnc *")
        lib.ppmd7_state_init(self.ppmd, max_order, mem_size, self._allocator)
        lib.ppmd7_compress_init(self.rc, self.writer)

    def encode(self, data) -> bytes:
        self.lock.acquire()
        in_buf = self._setup_inBuffer(data)
        out, out_buf = self._setup_outBuffer()
        while True:
            if lib.ppmd7_compress(self.ppmd, self.rc, out_buf, in_buf) == 0:
                break  # Finished
            # Output buffer should be exhausted, grow the buffer.
            if out_buf.pos == out_buf.size:
                out.grow(out_buf)
        self.lock.release()
        return out.finish(out_buf)

    def flush(self) -> bytes:
        if self.flushed:
            raise ("Ppmd7Encoder: Double flush error.")
        self.lock.acquire()
        self.flushed = True
        out, out_buf = self._setup_outBuffer()
        lib.ppmd7_compress_flush(self.rc)
        res = out.finish(out_buf)
        lib.ppmd7_state_close(self.ppmd, self._allocator)
        ffi.release(self.ppmd)
        self._release()
        ffi.release(self.rc)
        self.lock.release()
        return res

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        if not self.flushed:
            self.flush()


class Ppmd7Decoder(PpmdBaseDecoder):
    def __init__(self, max_order: int, mem_size: int):
        if mem_size > sys.maxsize:
            raise ValueError("Mem_size exceed to platform limit.")
        if _PPMD7_MIN_ORDER <= max_order <= _PPMD7_MAX_ORDER and _PPMD7_MIN_MEM_SIZE <= mem_size <= _PPMD7_MAX_MEM_SIZE:
            self.lock = Lock()
            self._init_common()
            self.ppmd = ffi.new("CPpmd7 *")
            self.rc = ffi.new("CPpmd7z_RangeDec *")
            self.flushed = False
            lib.ppmd7_state_init(self.ppmd, max_order, mem_size, self._allocator)
        else:
            raise ValueError("PPMd wrong parameters.")

    def decode(self, data: Union[bytes, bytearray, memoryview], length: int) -> bytes:
        if not isinstance(length, int) or length < 0:
            raise PpmdError("Wrong length argument is specified. It should be positive integer.")
        self.lock.acquire()
        in_buf, use_input_buffer = self._setup_inBuffer(data)
        if not self.inited:
            lib.ppmd7_decompress_init(self.rc, self.reader)
            self.inited = True
        out, out_buf = self._setup_outBuffer()
        remaining: int = length
        while remaining > 0:
            size = min(out_buf.size, remaining)
            out_size = lib.ppmd7_decompress(self.ppmd, self.rc, out_buf, in_buf, size)
            if out_buf.pos == out_buf.size:
                out.grow(out_buf)
            elif in_buf.pos == in_buf.size:
                break
            remaining = remaining - out_size
        self._unconsumed_in(in_buf, use_input_buffer)
        res = out.finish(out_buf)
        self.lock.release()
        return res

    def flush(self, length: int) -> bytes:
        if not isinstance(length, int) or length < 0:
            raise PpmdError("Wrong length argument is specified. It should be positive integer.")
        self.lock.acquire()
        if length > 0:
            in_buf, use_input_buffer = self._setup_inBuffer(b"")
            out, out_buf = self._setup_outBuffer()
            remaining: int = length
            while remaining > 0:
                if out_buf.pos == out_buf.size:
                    out.grow(out_buf)
                size = min(out_buf.size - out_buf.pos, remaining)
                lib.ppmd7_decompress_flush(self.ppmd, self.rc, out_buf, in_buf, size)
                remaining = remaining - size
            if self.rc.Code != 0:
                self.lock.release()
                raise ("PPMd decode error.")
            res = out.finish(out_buf)
        else:
            if self.rc.Code != 0:
                self.lock.release()
                raise ("PPMd decode error.")
            res = b""
        lib.ppmd7_state_close(self.ppmd, self._allocator)
        ffi.release(self.ppmd)
        ffi.release(self.rc)
        self._release()
        self.flushed = True
        self.lock.release()
        return res

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if not self.flushed:
            self.flush(0)


class Ppmd8Encoder(PpmdBaseEncoder):
    def __init__(self, max_order, mem_size):
        self.lock = Lock()
        if mem_size > sys.maxsize:
            raise ValueError("Mem_size exceed to platform limit.")
        self._init_common()
        self.ppmd = ffi.new("CPpmd8 *")
        lib.ppmd8_compress_init(self.ppmd, self.writer)
        lib.Ppmd8_Construct(self.ppmd)
        lib.Ppmd8_Alloc(self.ppmd, mem_size, self._allocator)
        lib.Ppmd8_RangeEnc_Init(self.ppmd)
        lib.Ppmd8_Init(self.ppmd, max_order, 0)

    def encode(self, data) -> bytes:
        self.lock.acquire()
        in_buf = self._setup_inBuffer(data)
        out, out_buf = self._setup_outBuffer()
        while lib.ppmd8_compress(self.ppmd, out_buf, in_buf) > 0:
            if out_buf.pos == out_buf.size:
                out.grow(out_buf)
        self.lock.release()
        return out.finish(out_buf)

    def flush(self) -> bytes:
        self.lock.acquire()
        if self.flushed:
            self.lock.release()
            return
        self.flushed = True
        out, out_buf = self._setup_outBuffer()
        lib.Ppmd8_EncodeSymbol(self.ppmd, -1)  # endmark
        lib.Ppmd8_RangeEnc_FlushData(self.ppmd)
        res = out.finish(out_buf)
        lib.Ppmd8_Free(self.ppmd, self._allocator)
        ffi.release(self.ppmd)
        self._release()
        self.lock.release()
        return res

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        if not self.flushed:
            self.flush()


class Ppmd8Decoder(PpmdBaseDecoder):
    def __init__(self, max_order: int, mem_size: int):
        self._init_common()
        self.ppmd = ffi.new("CPpmd8 *")
        lib.Ppmd8_Construct(self.ppmd)
        lib.ppmd8_decompress_init(self.ppmd, self.reader)
        lib.Ppmd8_Alloc(self.ppmd, mem_size, self._allocator)
        lib.Ppmd8_Init(self.ppmd, max_order, 0)
        self.inited = False
        self.flushed = False

    def _init2(self):
        lib.Ppmd8_RangeDec_Init(self.ppmd)

    def decode(self, data: Union[bytes, bytearray, memoryview], length: int):
        if not isinstance(length, int) or length < 0:
            raise PpmdError("Wrong length argument is specified. It should be positive integer.")
        self.lock.acquire()
        in_buf, use_input_buffer = self._setup_inBuffer(data)
        out, out_buf = self._setup_outBuffer()
        if not self.inited:
            self.inited = True
            self._init2()
        remaining: int = length
        while remaining > 0:
            if out_buf.pos == out_buf.size:
                out.grow(out_buf)
            size = min(out_buf.size, remaining)
            lib.ppmd8_decompress(self.ppmd, out_buf, in_buf, size)
            remaining = remaining - size
        self._unconsumed_in(in_buf, use_input_buffer)
        res = out.finish(out_buf)
        self.lock.release()
        return res

    def flush(self, length: int):
        if not isinstance(length, int) or length < 0:
            raise PpmdError("Wrong length argument is specified. It should be positive integer.")
        self.lock.acquire()
        if length > 0:
            in_buf, use_input_buffer = self._setup_inBuffer(b"")
            out, out_buf = self._setup_outBuffer()
            remaining: int = length
            while remaining > 0:
                if out_buf.pos == out_buf.size:
                    out.grow(out_buf)
                size = min(out_buf.size - out_buf.pos, remaining)
                lib.ppmd8_decompress_flush(self.ppmd, out_buf, in_buf, size)
                remaining = remaining - size
            self._unconsumed_in(in_buf, use_input_buffer)
            res = out.finish(out_buf)
        else:
            res = b""
        lib.Ppmd8_Free(self.ppmd, self._allocator)
        ffi.release(self.ppmd)
        self._release()
        self.flushed = True
        self.lock.release()
        return res

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if not self.flushed:
            self.flush(0)

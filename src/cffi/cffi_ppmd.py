import sys
from threading import Lock

from ._cffi_ppmd import ffi, lib

__all__ = ("Ppmd7Encoder", "Ppmd7Decoder")

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
        32*KB, 64*KB, 256*KB, 1*MB, 4*MB, 8*MB, 16*MB, 16*MB,
        32*MB, 32*MB, 32*MB, 32*MB, 64*MB, 64*MB, 128*MB, 128*MB,
        256*MB )
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
        if (len(self.list) == 1 and out.pos == out.size) or \
           (len(self.list) == 2 and out.pos == 0):
            return bytes(ffi.buffer(self.list[0]))

        # Final bytes object
        data_size = self.allocated - (out.size-out.pos)
        final = _new_nonzero("char[]", data_size)
        if final == ffi.NULL:
            raise MemoryError(self.MEM_ERR_MSG)

        # Memory copy
        # Blocks except the last one
        posi = 0
        for block in self.list[:-1]:
            ffi.memmove(final+posi, block, len(block))
            posi += len(block)
        # The last block
        ffi.memmove(final+posi, self.list[-1], out.pos)

        return bytes(ffi.buffer(final))

class Ppmd7Encoder:
    def __init__(self,  max_order: int, mem_size: int):
        self.lock = Lock()
        if mem_size > sys.maxsize:
            raise ValueError("Mem_size exceed to platform limit.")
        if _PPMD7_MIN_ORDER <= max_order <= _PPMD7_MAX_ORDER and _PPMD7_MIN_MEM_SIZE <= mem_size <= _PPMD7_MAX_MEM_SIZE:
            self.closed = False
            self.flushed = False
            self.ppmd = ffi.new("CPpmd7 *")
            self.rc = ffi.new("CPpmd7z_RangeEnc *")
            self.writer = ffi.new("BufferWriter *")
            self._allocator = ffi.new("ISzAlloc *")
            self._allocator.Alloc = lib.raw_alloc
            self._allocator.Free = lib.raw_free
            lib.ppmd_state_init(self.ppmd, max_order, mem_size, self._allocator)
            lib.ppmd_compress_init(self.rc, self.writer)
        else:
            raise ValueError("PPMd wrong parameters.")

    def encode(self, data) -> bytes:
        self.lock.acquire()

        # Input buffer
        in_buf = _new_nonzero("PPMD_inBuffer *")
        if in_buf == ffi.NULL:
            raise MemoryError
        in_buf.src = ffi.from_buffer(data)
        in_buf.size = len(data)
        in_buf.pos = 0

        # Output buffer
        out_buf = _new_nonzero("PPMD_outBuffer *")
        self.writer.outBuffer = out_buf
        if out_buf == ffi.NULL:
            raise MemoryError
        out = _BlocksOutputBuffer()

        # Initialize output buffer
        out.initAndGrow(out_buf, -1)

        while True:
            # Compress
            ret = lib.ppmd_compress(self.ppmd, self.rc, out_buf, in_buf)

            # Finished
            if ret == 0:
                self.lock.release()
                return out.finish(out_buf)

            # Output buffer should be exhausted, grow the buffer.
            if out_buf.pos == out_buf.size:
                out.grow(out_buf)


    def flush(self) -> bytes:
        if self.flushed:
            return
        self.flushed = True
        self.lock.acquire()

        # Output buffer
        out_buf = _new_nonzero("PPMD_outBuffer *")
        self.writer.outBuffer = out_buf
        if out_buf == ffi.NULL:
            raise MemoryError
        out = _BlocksOutputBuffer()

        # Initialize output buffer
        out.initAndGrow(out_buf, -1)

        lib.Ppmd7z_RangeEnc_FlushData(self.rc)
        self.lock.release()
        return out.finish(out_buf)

    def close(self):
        if self.closed:
            return
        self.lock.acquire()
        self.closed = True
        lib.ppmd_state_close(self.ppmd, self._allocator)
        ffi.release(self.ppmd)
        ffi.release(self.writer)
        ffi.release(self.rc)
        self.lock.release()

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        if not self.flushed:
            self.flush()
        self.close()


class Ppmd7Decoder:
    def __init__(self, max_order: int, mem_size: int):
        if mem_size > sys.maxsize:
            raise ValueError("Mem_size exceed to platform limit.")
        if _PPMD7_MIN_ORDER <= max_order <= _PPMD7_MAX_ORDER and _PPMD7_MIN_MEM_SIZE <= mem_size <= _PPMD7_MAX_MEM_SIZE:
            self.lock = Lock()
            self.ppmd = ffi.new("CPpmd7 *")
            self._allocator = ffi.new("ISzAlloc *")
            self._allocator.Alloc = lib.raw_alloc
            self._allocator.Free = lib.raw_free
            lib.ppmd_state_init(self.ppmd, max_order, mem_size, self._allocator)
            self.rc = ffi.new("CPpmd7z_RangeDec *")
            self.reader = ffi.new("BufferReader *")
            self.closed = False
            self.inited = False
            self._input_buffer = ffi.NULL
            self._input_buffer_size = 0
            self._in_begin = 0
            self._in_end = 0
        else:
            raise ValueError("PPMd wrong parameters.")

    def decode(self, data, length) -> bytes:
        self.lock.acquire()

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

            assert (used_now > 0
                    and avail_now >= 0
                    and avail_total >= 0)

            if avail_total < len(data):
                new_size = used_now + len(data)
                # Allocate with new size
                tmp = _new_nonzero("char[]", new_size)
                if tmp == ffi.NULL:
                    raise MemoryError

                # Copy unconsumed data to the beginning of new buffer
                ffi.memmove(tmp,
                            self._input_buffer+self._in_begin,
                            used_now)

                # Switch to new buffer
                self._input_buffer = tmp
                self._input_buffer_size = new_size

                # Set begin & end position
                self._in_begin = 0
                self._in_end = used_now
            elif avail_now < len(data):
                # Move unconsumed data to the beginning
                ffi.memmove(self._input_buffer,
                            self._input_buffer+self._in_begin,
                            used_now)

                # Set begin & end position
                self._in_begin = 0
                self._in_end = used_now

            # Copy data to input buffer
            ffi.memmove(self._input_buffer+self._in_end,
                        ffi.from_buffer(data), len(data))
            self._in_end += len(data)

            in_buf.src = self._input_buffer + self._in_begin
            in_buf.size = used_now + len(data)
            in_buf.pos = 0
        # Now in_buf.pos == 0

        self.reader.inBuffer = in_buf

        if not self.inited:
            lib.ppmd_decompress_init(self.rc, self.reader)
            self.inited = True

        # Output buffer
        out_buf = _new_nonzero("PPMD_outBuffer *")
        if out_buf == ffi.NULL:
            raise MemoryError
        out = _BlocksOutputBuffer()

        # Initialize output buffer
        out.initAndGrow(out_buf, -1)

        remaining: int = length

        while remaining > 0:
            if out_buf.pos == out_buf.size:
                out.grow(out_buf)
            size = min(out_buf.size, remaining)
            lib.ppmd_decompress(self.ppmd, self.rc, out_buf, in_buf, size)
            remaining = remaining - size

        # Unconsumed input data
        if in_buf.pos == in_buf.size:
            if use_input_buffer:
                # Clear input_buffer
                self._in_begin = 0
                self._in_end = 0
        else:
            data_size = in_buf.size - in_buf.pos
            if not use_input_buffer:
                # Discard buffer if it's too small
                if (self._input_buffer == ffi.NULL
                      or self._input_buffer_size < data_size):
                    # Create new buffer
                    self._input_buffer = _new_nonzero("char[]", data_size)
                    if self._input_buffer == ffi.NULL:
                        self._input_buffer_size = 0
                        raise MemoryError
                    # Set buffer size
                    self._input_buffer_size = data_size

                # Copy unconsumed data
                ffi.memmove(self._input_buffer, in_buf.src+in_buf.pos, data_size)
                self._in_begin = 0
                self._in_end = data_size
            else:
                # Use input buffer
                self._in_begin += in_buf.pos

        if self.rc.Code != 0:
            pass  # FIXME
        self.lock.release()
        return out.finish(out_buf);

    def close(self):
        if self.closed:
            return
        self.lock.acquire()
        lib.ppmd_state_close(self.ppmd, self._allocator)
        ffi.release(self.ppmd)
        ffi.release(self.reader)
        ffi.release(self.rc)
        self.closed = True
        self.lock.release()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

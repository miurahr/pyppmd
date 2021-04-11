import sys
from typing import BinaryIO

from ._cffi_ppmd import ffi, lib

__all__ = ("Ppmd7Encoder", "Ppmd7Decoder")

_PPMD7_MIN_ORDER = 2
_PPMD7_MAX_ORDER = 64

_PPMD7_MIN_MEM_SIZE = 1 << 11
_PPMD7_MAX_MEM_SIZE = 0xFFFFFFFF - 12 * 3


@ffi.def_extern()
def dst_write(b: bytes, size: int, userdata: object) -> None:
    encoder = ffi.from_handle(userdata)
    buf = ffi.buffer(b, size)
    encoder.destination.write(buf)


@ffi.def_extern()
def src_readinto(b: bytes, size: int, userdata: object) -> int:
    decoder = ffi.from_handle(userdata)
    buf = ffi.buffer(b, size)
    result = decoder.source.readinto(buf)
    return result


_allocated = []


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


class Ppmd7Decoder:
    def __init__(self, destination: BinaryIO, max_order: int, mem_size: int):
        if mem_size > sys.maxsize:
            raise ValueError("Mem_size exceed to platform limit.")
        if _PPMD7_MIN_ORDER <= max_order <= _PPMD7_MAX_ORDER and _PPMD7_MIN_MEM_SIZE <= mem_size <= _PPMD7_MAX_MEM_SIZE:
            self.closed = False
            self.flushed = False
            self.destination = destination
            self.ppmd = ffi.new("CPpmd7 *")
            self.rc = ffi.new("CPpmd7z_RangeEnc *")
            self.writer = ffi.new("RawWriter *")
            self._allocator = ffi.new("ISzAlloc *")
            self._allocator.Alloc = lib.raw_alloc
            self._allocator.Free = lib.raw_free
            self._userdata = ffi.new_handle(self)
            lib.ppmd_state_init(self.ppmd, max_order, mem_size, self._allocator)
            lib.ppmd_compress_init(self.rc, self.writer, lib.dst_write, self._userdata)
        else:
            raise ValueError("PPMd wrong parameters.")

    def encode(self, inbuf) -> None:
        for sym in inbuf:
            lib.Ppmd7_EncodeSymbol(self.ppmd, self.rc, sym)

    def flush(self):
        if self.flushed:
            return
        self.flushed = True
        lib.Ppmd7z_RangeEnc_FlushData(self.rc)

    def close(self):
        if self.closed:
            return
        self.closed = True
        lib.ppmd_state_close(self.ppmd, self._allocator)
        ffi.release(self.ppmd)
        ffi.release(self.writer)
        ffi.release(self.rc)

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        if not self.flushed:
            self.flush()
        self.close()


class Ppmd7Encoder:
    def __init__(self, source: BinaryIO, max_order: int, mem_size: int):
        if not source.readable:
            raise ValueError("Source stream is not readable")
        if mem_size > sys.maxsize:
            raise ValueError("Mem_size exceed to platform limit.")
        if _PPMD7_MIN_ORDER <= max_order <= _PPMD7_MAX_ORDER and _PPMD7_MIN_MEM_SIZE <= mem_size <= _PPMD7_MAX_MEM_SIZE:
            self.ppmd = ffi.new("CPpmd7 *")
            self._allocator = ffi.new("ISzAlloc *")
            self._allocator.Alloc = lib.raw_alloc
            self._allocator.Free = lib.raw_free
            lib.ppmd_state_init(self.ppmd, max_order, mem_size, self._allocator)
            self.rc = ffi.new("CPpmd7z_RangeDec *")
            self.reader = ffi.new("RawReader *")
            self.source = source  # read indirectly through self._userdata
            self._userdata = ffi.new_handle(self)
            self.closed = False
            lib.ppmd_decompress_init(self.rc, self.reader, lib.src_readinto, self._userdata)
        else:
            raise ValueError("PPMd wrong parameters.")

    def decode(self, length) -> bytes:
        b = bytearray()
        for _ in range(length):
            sym = lib.Ppmd7_DecodeSymbol(self.ppmd, self.rc)
            b += sym.to_bytes(1, "little")
        if self.rc.Code != 0:
            pass  # FIXME
        return bytes(b)

    def close(self):
        if self.closed:
            return
        lib.ppmd_state_close(self.ppmd, self._allocator)
        ffi.release(self.ppmd)
        ffi.release(self.reader)
        ffi.release(self.rc)
        self.closed = True

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

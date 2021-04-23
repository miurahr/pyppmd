from typing import Union

try:
    from importlib.metadata import PackageNotFoundError, version
except ImportError:
    from importlib_metadata import PackageNotFoundError, version  # type: ignore  # noqa

try:
    from .c.c_ppmd import *  # noqa
except ImportError:
    try:
        from .cffi.cffi_ppmd import *  # noqa
    except ImportError:
        msg = "pyppmd module: Neither C implementation nor CFFI " "implementation can be imported."
        raise ImportError(msg)

__all__ = ("compress", "decompress", "Ppmd7Encoder", "Ppmd7Decoder", "Ppmd8Encoder", "Ppmd8Decoder", "PpmdError")

__doc__ = """\
Python bindings to PPMd compression library, the API is similar to
Python's bz2/lzma/zlib module.

Documentation: https://pyppmd.readthedocs.io
Github: https://github.com/miurahr/pyppmd
PyPI: httpsrecursive-include src *.pyi://pypi.org/prject/pyppmd"""

__copyright__ = "Copyright (C) 2020,2021 Hiroshi Miura"

try:
    __version__ = version(__name__)
except PackageNotFoundError:  # pragma: no-cover
    # package is not installed
    __version__ = "unknown"


def compress(data: Union[bytes, bytearray, memoryview], max_order: int = 6, mem_size: int = 16 << 20) -> bytes:
    """Compress a block of data, return a bytes object.

    Arguments
    data:        A bytes-like object, data to be compressed.
    max_order:   An integer object represent compression level.
    mem_size:    An integer object represent memory size to use.
    """
    comp = Ppmd7Encoder(max_order, mem_size)
    result = comp.encode(data)
    return result + comp.flush()


def decompress(
    data: Union[bytes, bytearray, memoryview], length: int, max_order: int = 6, mem_size: int = 16 << 20
) -> bytes:
    """Decompress a PPMd data, return a bytes object.

    Arguments
    data:      A bytes-like object, compressed data.
    length:    A size of uncompressed data.
    max_order: An integer object represent max order of PPMd.
    mem_size:  An integer object represent memory size to use.
    """
    decomp = Ppmd7Decoder(max_order, mem_size)
    res = decomp.decode(data, length)
    return res + decomp.flush(length - len(res))

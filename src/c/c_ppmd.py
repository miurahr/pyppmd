from ._ppmd import Ppmd7Decoder, Ppmd7Encoder

__all__ = ("compress", "decompress", "Ppmd7Encoder", "Ppmd7Decoder", "PpmdError")


class PpmdError(Exception):
    "Call to the underlying PPMd library failed."
    pass


def compress(data, max_order: int = 6, mem_size: int = 16 << 20) -> bytes:
    """Compress a block of data, return a bytes object.

    Arguments
    data:        A bytes-like object, data to be compressed.
    max_order:   An integer object represent compression level.
    mem_size:    An integer object represent memory size to use.
    """
    comp = Ppmd7Encoder(max_order, mem_size)
    result = comp.compress(data)
    return result + comp.flush()


def decompress(data, length, max_order: int = 6, mem_size: int = 16 << 20) -> bytes:
    """Decompress a PPMd data, return a bytes object.

    Arguments
    data:      A bytes-like object, compressed data.
    length:    A size of uncompressed data.
    max_order: An integer object represent max order of PPMd.
    mem_size:  An integer object represent memory size to use.
    """
    decomp = Ppmd7Decoder(max_order, mem_size)
    return decomp.decompress(data, length)

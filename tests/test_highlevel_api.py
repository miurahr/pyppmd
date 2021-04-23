import pyppmd

source = b"This file is located in a folder.This file is located in the root."
encoded = b"\x00T\x16C\x98\xbdi\x9b\n\xf1B^N\xac\xc8}:\xbak&\xc1\x7f\x01p\xc51C\xb0b\x1b@\x9a\xb6h\x9a-0\x98\xc0\\'"
READ_BLOCKSIZE = 16384


def test_simple_compress():
    assert pyppmd.compress(source, 6, 8 << 20) == encoded


def test_simple_decompress():
    assert pyppmd.decompress(encoded, len(source), 6, 8 << 20) == source

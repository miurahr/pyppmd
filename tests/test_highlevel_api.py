import pyppmd

source = "This file is located in a folder.This file is located in the root.\n"
encoded = (
    b"\x54\x16\x43\x6d\x5c\xd8\xd7\x3a\xb3\x58\x31\xac\x1d\x09\x23\xfd\x11\xd5\x72\x62\x73"
    b"\x13\xb6\xce\xb2\xe7\x6a\xb9\xf6\xe8\x66\xf5\x08\xc3\x0a\x09\x36\x12\xeb\xda\xda\xba"
)
READ_BLOCKSIZE = 16384


# Test one-shot functions
def test_compress_str():
    assert pyppmd.compress(source, max_order=6, mem_size=8 << 20) == encoded


def test_compress():
    assert pyppmd.compress(source.encode("UTF-8"), max_order=6, mem_size=8 << 20) == encoded


def test_decompress_str():
    assert pyppmd.decompress_str(encoded, max_order=6, mem_size=8 << 20, encoding="UTF-8") == source


def test_decompress():
    assert pyppmd.decompress(encoded, max_order=6, mem_size=8 << 20) == source.encode("UTF-8")

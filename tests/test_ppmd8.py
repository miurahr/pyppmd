import hashlib
import os
import pathlib

import pytest

import pyppmd

testdata_path = pathlib.Path(os.path.dirname(__file__)).joinpath("data")
source = b"This file is located in a folder.This file is located in the root.\n"
encoded = (
    b"\x54\x16\x43\x6d\x5c\xd8\xd7\x3a\xb3\x58\x31\xac\x1d\x09\x23\xfd\x11\xd5\x72\x62\x73"
    b"\x13\xb6\xce\xb2\xe7\x6a\xb9\xf6\xe8\x66\xf5\x08\xc3\x0a\x09\x36\x12\xeb\xda\xda\xba"
)

READ_BLOCKSIZE = 16384


def test_ppmd8_encoder1():
    encoder = pyppmd.Ppmd8Encoder(6, 8 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART)
    result = encoder.encode(source)
    result += encoder.flush()
    assert result == encoded


def test_ppmd8_encoder2():
    encoder = pyppmd.Ppmd8Encoder(6, 8 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART)
    result = encoder.encode(source[:33])
    result += encoder.encode(source[33:])
    result += encoder.flush()
    assert result == encoded


def test_ppmd8_decoder1():
    decoder = pyppmd.Ppmd8Decoder(6, 8 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART)
    result = decoder.decode(encoded, -1)
    result += decoder.decode(b"", -1)
    assert result == source
    # assert decoder.eof and not decoder.needs_input


def test_ppmd8_decoder2():
    decoder = pyppmd.Ppmd8Decoder(6, 8 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART)
    result = decoder.decode(encoded[:20])
    result += decoder.decode(encoded[20:])
    result += decoder.decode(b"", -1)
    assert result == source
    # assert decoder.eof and not decoder.needs_input


# test mem_size less than original file size as well
@pytest.mark.parametrize(
    "mem_size, restore_method",
    [
        (8 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART),
        (8 << 20, pyppmd.PPMD8_RESTORE_METHOD_CUT_OFF),
        (1 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART),
        (1 << 20, pyppmd.PPMD8_RESTORE_METHOD_CUT_OFF),
    ],
)
@pytest.mark.timeout(20)
def test_ppmd8_encode_decode(tmp_path, mem_size, restore_method):
    length = 0
    m = hashlib.sha256()
    with testdata_path.joinpath("10000SalesRecords.csv").open("rb") as f:
        with tmp_path.joinpath("target.ppmd").open("wb") as target:
            enc = pyppmd.Ppmd8Encoder(6, mem_size, restore_method=restore_method)
            data = f.read(READ_BLOCKSIZE)
            while len(data) > 0:
                m.update(data)
                length += len(data)
                target.write(enc.encode(data))
                data = f.read(READ_BLOCKSIZE)
            target.write(enc.flush(endmark=True))
    shash = m.digest()
    m2 = hashlib.sha256()
    assert length == 1237262
    length = 0
    with tmp_path.joinpath("target.ppmd").open("rb") as target:
        with tmp_path.joinpath("target.csv").open("wb") as out:
            dec = pyppmd.Ppmd8Decoder(6, mem_size, restore_method=restore_method)
            data = target.read(READ_BLOCKSIZE)
            while not dec.eof:
                res = dec.decode(data)
                m2.update(res)
                out.write(res)
                length += len(res)
                if len(data) == 0:
                    break
                data = target.read(READ_BLOCKSIZE)
    assert length == 1237262
    thash = m2.digest()
    assert thash == shash


def test_ppmd8_encode_decode_shortage():
    txt = "\U0001127f\U00069f6a\U00069f6a"
    obj = txt.encode("UTF-8")
    enc = pyppmd.Ppmd8Encoder(3, 2048)
    data = enc.encode(obj)
    data += enc.flush()
    length = len(obj)
    dec = pyppmd.Ppmd8Decoder(3, 2048)
    res = dec.decode(data, length)
    if len(res) < length:
        res += dec.decode(b"\0", length - len(res))
    assert obj == res


def test_ppmdcompress():
    compressor = pyppmd.PpmdCompressor(6, 8 << 20, restore_method=pyppmd.PPMD8_RESTORE_METHOD_RESTART, variant="I")
    result = compressor.compress(source)
    result += compressor.flush()
    assert result == encoded


def test_ppmddecompress():
    decomp = pyppmd.PpmdDecompressor(6, 8 << 20, restore_method=pyppmd.PPMD8_RESTORE_METHOD_RESTART, variant="I")
    result = decomp.decompress(encoded)
    assert result == source

import hashlib
import os
import pathlib

import pytest

import pyppmd

testdata_path = pathlib.Path(os.path.dirname(__file__)).joinpath("data")
data = b"This file is located in a folder.This file is located in the root."
encoded = b"\x00T\x16C\x98\xbdi\x9b\n\xf1B^N\xac\xc8}:\xbak&\xc1\x7f\x01p\xc51C\xb0b\x1b@\x9a\xb6h\x9a-0\x98\xc0\\'"
READ_BLOCKSIZE = 16384


def test_ppmd7_encoder():
    encoder = pyppmd.Ppmd7Encoder(6, 16 << 20)
    result = encoder.encode(data)
    result += encoder.flush()
    assert len(result) == 41
    assert result == encoded


def test_ppmd7_encoder2():
    encoder = pyppmd.Ppmd7Encoder(6, 16 << 20)
    result = encoder.encode(data[:33])
    result += encoder.encode(data[33:])
    result += encoder.flush(endmark=False)
    assert len(result) == 41
    assert result == encoded


def test_ppmd7_decoder():
    decoder = pyppmd.Ppmd7Decoder(6, 16 << 20)
    result = decoder.decode(encoded, 66)
    assert result == data
    assert decoder.eof
    assert not decoder.needs_input


def test_ppmd7_decoder2():
    decoder = pyppmd.Ppmd7Decoder(6, 16 << 20)
    result = decoder.decode(encoded[:33], 33)
    result += decoder.decode(encoded[33:], 28)
    assert not decoder.eof
    while len(result) < 66:
        if decoder.needs_input:
            result += decoder.decode(b"\0", 66 - len(result))
            break
        else:
            result += decoder.decode(b"", 66 - len(result))
    assert result == data
    assert not decoder.needs_input
    assert decoder.eof


# test mem_size less than original file size as well
@pytest.mark.parametrize("mem_size", [(16 << 20), (1 << 20)])
def test_ppmd7_encode_decode(tmp_path, mem_size):
    length = 0
    m = hashlib.sha256()
    with testdata_path.joinpath("10000SalesRecords.csv").open("rb") as f:
        with tmp_path.joinpath("target.ppmd").open("wb") as target:
            enc = pyppmd.Ppmd7Encoder(6, mem_size)
            data = f.read(READ_BLOCKSIZE)
            while len(data) > 0:
                m.update(data)
                length += len(data)
                target.write(enc.encode(data))
                data = f.read(READ_BLOCKSIZE)
            target.write(enc.flush())
    shash = m.digest()
    m2 = hashlib.sha256()
    assert length == 1237262
    remaining = length
    with tmp_path.joinpath("target.ppmd").open("rb") as target:
        with tmp_path.joinpath("target.csv").open("wb") as out:
            dec = pyppmd.Ppmd7Decoder(6, mem_size)
            while remaining > 0:
                data = target.read(READ_BLOCKSIZE)
                res = dec.decode(data, min(remaining, READ_BLOCKSIZE))
                if len(res) == 0:
                    if dec.needs_input:
                        res += dec.decode(b"\0", remaining)
                    else:
                        res += dec.decode(b"", remaining)
                    break
                remaining -= len(res)
                m2.update(res)
                out.write(res)
            assert remaining == 0
        thash = m2.digest()
    assert thash == shash


chunk_sizes = [
    189703,
    198694,
    189694,
    189742,
    189776,
    189823,
    189690,
    189723,
    189724,
    189766,
    189751,
    189778,
    189825,
    189835,
    189805,
    189820,
    189810,
    189776,
    189779,
    189776,
    189833,
    189880,
    189857,
    189823,
    189846,
    189848,
    189887,
    189847,
    189870,
    189857,
    189888,
    189943,
    189900,
    189915,
    189940,
    189932,
    189912,
    189970,
    189943,
    189967,
    189999,
    189990,
    189947,
    189944,
    189982,
    189987,
    189962,
    189956,
    189953,
    189960,
]


def test_ppmd7_decode_chunks():
    with testdata_path.joinpath("testdata2.ppmd").open("rb") as f:
        dec = pyppmd.Ppmd7Decoder(6, 16 << 20)
        for i in range(30):
            remaining = chunk_sizes[i]
            result = b""
            while remaining > 0:
                data = f.read(READ_BLOCKSIZE)
                out = dec.decode(data, remaining)
                if len(out) == 0:
                    if dec.needs_input:
                        out += dec.decode(b"\0", remaining)
                    else:
                        out += dec.decode(b"", remaining)
                    break
                remaining -= len(out)
                result += out
            assert len(result) == chunk_sizes[i]

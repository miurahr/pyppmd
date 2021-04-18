import hashlib
import os
import pathlib

import pyppmd

testdata_path = pathlib.Path(os.path.dirname(__file__)).joinpath("data")
data = b"This file is located in a folder.This file is located in the root."
READ_BLOCKSIZE = 16384


def test_ppmd7_encoder():
    encoder = pyppmd.Ppmd7Encoder(6, 16 << 20)
    result = encoder.encode(data)
    result += encoder.flush()
    assert len(result) == 41
    with testdata_path.joinpath("ppmd7.dat").open("rb") as f:
        assert bytes(result) == f.read()


def test_ppmd7_encoder2():
    encoder = pyppmd.Ppmd7Encoder(6, 16 << 20)
    result = encoder.encode(data[:33])
    result += encoder.encode(data[33:])
    result += encoder.flush()
    assert len(result) == 41
    with testdata_path.joinpath("ppmd7.dat").open("rb") as f:
        assert result == f.read()


def test_ppmd7_decoder():
    decoder = pyppmd.Ppmd7Decoder(6, 16 << 20)
    with testdata_path.joinpath("ppmd7.dat").open("rb") as f:
        result = decoder.decode(f.read(41), 66)
        result += decoder.flush(66 - len(result))
        assert result == data


def test_ppmd7_decoder2():
    decoder = pyppmd.Ppmd7Decoder(6, 16 << 20)
    with testdata_path.joinpath("ppmd7.dat").open("rb") as f:
        result = decoder.decode(f.read(33), 33)
        result += decoder.decode(f.read(8), 28)
        result += decoder.flush(66 - len(result))
        assert result == data


def test_ppmd7_encode_decode(tmp_path):
    length = 0
    m = hashlib.sha256()
    with testdata_path.joinpath("10000SalesRecords.csv").open("rb") as f:
        with tmp_path.joinpath("target.ppmd").open("wb") as target:
            enc = pyppmd.Ppmd7Encoder(6, 16 << 20)
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
            dec = pyppmd.Ppmd7Decoder(6, 16 << 20)
            while remaining > 0:
                data = target.read(READ_BLOCKSIZE)
                if len(data) == 0:
                    res = dec.flush(remaining)
                else:
                    res = dec.decode(data, min(remaining, READ_BLOCKSIZE))
                remaining -= len(res)
                m2.update(res)
                out.write(res)
            assert remaining == 0
        thash = m2.digest()
    assert thash == shash

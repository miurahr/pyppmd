import io
import os
import pathlib

import pytest

import pyppmd

testdata_path = pathlib.Path(os.path.join(os.path.dirname(__file__), "data"))
testdata = testdata_path.joinpath("10000SalesRecords.csv")
src_size = testdata.stat().st_size
READ_BLOCKSIZE = 1048576

targets = [
    ("PPMd H", 7, 6, 16 << 20),
    ("PPMd I", 8, 8, 8 << 20),
]


@pytest.mark.benchmark(group="compress")
@pytest.mark.parametrize("name, var, max_order, mem_size", targets)
def test_benchmark_text_compress(tmp_path, benchmark, name, var, max_order, mem_size):
    def encode(var, max_order, mem_size):
        if var == 7:
            encoder = pyppmd.Ppmd7Encoder(max_order=max_order, mem_size=mem_size)
        else:
            encoder = pyppmd.Ppmd8Encoder(max_order=max_order, mem_size=mem_size)
        with io.BytesIO() as target:
            with testdata.open("rb") as src:
                data = src.read(READ_BLOCKSIZE)
                while len(data) > 0:
                    target.write(encoder.encode(data))
                    data = src.read(READ_BLOCKSIZE)
                target.write(encoder.flush())

    benchmark.extra_info["data_size"] = src_size
    benchmark(encode, var, max_order, mem_size)


@pytest.mark.benchmark(group="decompress")
@pytest.mark.parametrize("name, var, max_order, mem_size", targets)
def test_benchmark_text_decompress(tmp_path, benchmark, name, var, max_order, mem_size):
    def decode(var, max_order, mem_size):
        if var == 7:
            decoder = pyppmd.Ppmd7Decoder(max_order=max_order, mem_size=mem_size)
        else:
            decoder = pyppmd.Ppmd8Decoder(max_order=max_order, mem_size=mem_size)
        with tmp_path.joinpath("target.ppmd").open("rb") as src:
            with io.BytesIO() as target:
                remaining = src_size
                data = src.read(READ_BLOCKSIZE)
                while remaining > 0:
                    if len(data) == 0:
                        target.write(decoder.flush(remaining))
                        break
                    else:
                        out = decoder.decode(data, remaining)
                    target.write(out)
                    remaining = remaining - len(out)
                    data = src.read(READ_BLOCKSIZE)

    # prepare compressed data
    if var == 7:
        encoder = pyppmd.Ppmd7Encoder(max_order=max_order, mem_size=mem_size)
    else:
        encoder = pyppmd.Ppmd8Encoder(max_order=max_order, mem_size=mem_size)
    with tmp_path.joinpath("target.ppmd").open("wb") as target:
        with testdata.open("rb") as src:
            data = src.read(READ_BLOCKSIZE)
            while len(data) > 0:
                target.write(encoder.encode(data))
                data = src.read(READ_BLOCKSIZE)
            target.write(encoder.flush())

    benchmark.extra_info["data_size"] = src_size
    benchmark(decode, var, max_order, mem_size)

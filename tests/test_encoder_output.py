"""Regression tests: the encoder must not drop bytes when its output buffer fills up.

Writer() used to discard bytes once the current output block was full, because
the encode loop only grew the buffer after a whole symbol had been encoded and
the range encoder can emit more than one byte per symbol.  Every encode() call
starts a fresh 32 KiB block, so feeding chunks that compress to a little more
than that crosses a block boundary on every call.
"""

import random

import pytest

import pyppmd

ORDER = 6
MEM = 16 << 20
CHUNK = 40_000  # incompressible input: ~40 KB of output per call, over the 32 KiB first block
CALLS = 300


@pytest.fixture(scope="module")
def payload():
    return random.Random(20261101).randbytes(CHUNK * CALLS)


def _encode(encoder, chunks, **flush_kwargs):
    out = b"".join(encoder.encode(c) for c in chunks)
    return out + encoder.flush(**flush_kwargs)


@pytest.mark.parametrize(
    "encoder_cls, decoder_cls",
    [(pyppmd.Ppmd7Encoder, pyppmd.Ppmd7Decoder), (pyppmd.Ppmd8Encoder, pyppmd.Ppmd8Decoder)],
    ids=["ppmd7", "ppmd8"],
)
def test_chunked_encode_matches_bulk(payload, encoder_cls, decoder_cls):
    chunks = [payload[i : i + CHUNK] for i in range(0, len(payload), CHUNK)]
    bulk = _encode(encoder_cls(ORDER, MEM), [payload], endmark=True)
    chunked = _encode(encoder_cls(ORDER, MEM), chunks, endmark=True)
    assert len(chunked) == len(bulk)
    assert chunked == bulk
    assert decoder_cls(ORDER, MEM).decode(chunked, len(payload)) == payload


@pytest.mark.parametrize("chunk", [1, 7, 4096, 32 * 1024, 32 * 1024 + 1, 100_000])
def test_ppmd7_chunk_sizes(payload, chunk):
    data = payload[: 2 * 1024 * 1024 if chunk >= 4096 else 128 * 1024]
    chunks = [data[i : i + chunk] for i in range(0, len(data), chunk)]
    bulk = _encode(pyppmd.Ppmd7Encoder(ORDER, MEM), [data], endmark=True)
    assert _encode(pyppmd.Ppmd7Encoder(ORDER, MEM), chunks, endmark=True) == bulk

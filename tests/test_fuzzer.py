import sys
from datetime import timedelta

from hypothesis import given, settings
from hypothesis import strategies as st

import pyppmd

if sys.maxsize > 2**32:
    MAX_SIZE = 1 << 30
else:
    MAX_SIZE = 1 << 28


# Ppmd7Encoder.encode() can silently drop a byte when a single call encodes a large input,
# see the encode() note in tests/data/ppmd7_golden/generate.py.  Encode in 16 KiB chunks to
# keep this test about the decoder.
PPMD7_ENCODE_CHUNK = 16384


@given(
    obj=st.one_of(st.text(min_size=1).map(lambda s: s.encode("utf-8")), st.binary(min_size=1)),
    max_order=st.integers(min_value=2, max_value=64),
    mem_size=st.integers(min_value=1 << 11, max_value=MAX_SIZE),
    data=st.data(),
)
@settings(deadline=timedelta(milliseconds=300))
def test_ppmd7_fuzzer(obj, max_order, mem_size, data):
    enc = pyppmd.Ppmd7Encoder(max_order=max_order, mem_size=mem_size)
    length = len(obj)
    compressed = b"".join(enc.encode(obj[i : i + PPMD7_ENCODE_CHUNK]) for i in range(0, length, PPMD7_ENCODE_CHUNK))
    compressed += enc.flush()
    # single call
    dec = pyppmd.Ppmd7Decoder(max_order=max_order, mem_size=mem_size)
    result = dec.decode(compressed, length, final=True)
    assert result == obj
    assert not dec.needs_input
    # split at drawn points; the first call needs at least 5 bytes
    points = data.draw(st.lists(st.integers(min_value=5, max_value=len(compressed)), max_size=8), label="split points")
    bounds = [0] + sorted(set(points) - {len(compressed)}) + [len(compressed)]
    chunks = [compressed[a:b] for a, b in zip(bounds, bounds[1:])]
    dec = pyppmd.Ppmd7Decoder(max_order=max_order, mem_size=mem_size)
    result = b""
    for i, chunk in enumerate(chunks):
        result += dec.decode(chunk, length - len(result), final=i == len(chunks) - 1)
    assert result == obj
    assert not dec.needs_input


@given(
    txt=st.text(min_size=1),
    max_order=st.integers(min_value=3, max_value=16),
    mem_size=st.integers(min_value=1 << 11, max_value=MAX_SIZE),
)
@settings(deadline=timedelta(milliseconds=300))
def test_ppmd8_fuzzer(txt, max_order, mem_size):
    obj = txt.encode("UTF-8")
    enc = pyppmd.Ppmd8Encoder(max_order=max_order, mem_size=mem_size, restore_method=pyppmd.PPMD8_RESTORE_METHOD_CUT_OFF)
    length = len(obj)
    compressed = enc.encode(obj)
    compressed += enc.flush()
    dec = pyppmd.Ppmd8Decoder(max_order=max_order, mem_size=mem_size, restore_method=pyppmd.PPMD8_RESTORE_METHOD_CUT_OFF)
    result = dec.decode(compressed, length)
    if len(result) < length:
        if dec.needs_input:
            # ppmd need extra null byte
            result += dec.decode(b"\0", length - len(result))
        else:
            result += dec.decode(b"", length - len(result))
    assert result == obj


if __name__ == "__main__":
    import atheris  # type: ignore  # noqa

    atheris.Setup(sys.argv, test_ppmd7_fuzzer.hypothesis.fuzz_one_input)
    atheris.Fuzz()

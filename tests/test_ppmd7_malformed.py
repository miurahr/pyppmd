"""Malformed-input properties of Ppmd7Decoder.

The default hypothesis profile keeps this file fast enough for CI. For a long run use
``pytest tests/test_ppmd7_malformed.py --hypothesis-profile=fuzz`` (see conftest.py),
preferably under the sanitizer build of utils/asan_test.sh.
"""

from hypothesis import event, given, settings
from hypothesis import strategies as st

import pyppmd

ALLOWED_ERRORS = (ValueError, RuntimeError, pyppmd.PpmdError)

# Ppmd7Encoder.encode() can silently drop a byte when one call crosses an output block
# boundary (see the encode() note in tests/data/ppmd7_golden/generate.py).  Encode in
# chunks of 16 KiB or less so that the streams below are valid before mutation.
ENCODE_CHUNK = 16384

max_orders = st.integers(min_value=2, max_value=64)
mem_sizes = st.integers(min_value=2 << 10, max_value=16 << 20)
lengths = st.one_of(st.integers(min_value=0, max_value=1 << 20), st.just(-1))


# Ppmd7z_RangeDec_Init() rejects a stream unless its first byte is 0, and the first decode()
# call needs at least 5 bytes.  Most examples pass both checks so that they reach the symbol
# decoder; the rest stay fully arbitrary.
streams = st.one_of(
    st.binary(min_size=4, max_size=4096).map(lambda b: b"\0" + b),
    st.binary(max_size=4096),
)


def split(data, stream):
    """Split `stream` at drawn points into at least one chunk (empty chunks allowed).

    The first chunk usually holds at least the 5 byte header.
    """
    first = data.draw(st.sampled_from([min(5, len(stream)), 0]), label="first split point minimum")
    points = data.draw(st.lists(st.integers(min_value=first, max_value=len(stream)), max_size=8), label="split points")
    bounds = [0] + sorted(points) + [len(stream)]
    return [stream[a:b] for a, b in zip(bounds, bounds[1:])]


def feed(dec, chunks, length):
    """Feed chunks with final=False except the last one, check the per-call invariants.

    Returns the collected output, or None when the decoder raised an allowed error.
    """
    result = b""
    for i, chunk in enumerate(chunks):
        last = i == len(chunks) - 1
        remaining = length - len(result) if length >= 0 else -1
        try:
            out = dec.decode(chunk, remaining, final=last)
        except ALLOWED_ERRORS as e:
            event(f"raised {type(e).__name__}")
            return None
        assert isinstance(out, bytes)
        if remaining >= 0:
            assert len(out) <= remaining
        result += out
    assert not dec.needs_input
    event("eof" if dec.eof else "returned")
    return result


@given(
    stream=streams,
    max_order=max_orders,
    mem_size=mem_sizes,
    length=lengths,
    data=st.data(),
)
@settings(deadline=None)
def test_arbitrary_bytes(stream, max_order, mem_size, length, data):
    """Arbitrary bytes as a compressed stream: only allowed errors, output bounded by length.

    With length=-1 and no end mark the output is unspecified; only termination is checked.
    """
    dec = pyppmd.Ppmd7Decoder(max_order, mem_size)
    result = feed(dec, split(data, stream), length)
    if result is not None and length >= 0:
        assert len(result) <= length


@given(
    plain=st.binary(min_size=1, max_size=40000),
    max_order=max_orders,
    mem_size=mem_sizes,
    endmark=st.booleans(),
    data=st.data(),
)
@settings(deadline=None)
def test_mutated_stream(plain, max_order, mem_size, endmark, data):
    """A valid stream with a few bytes flipped, inserted or deleted must not crash the decoder."""
    enc = pyppmd.Ppmd7Encoder(max_order, mem_size)
    stream = b"".join(enc.encode(plain[i : i + ENCODE_CHUNK]) for i in range(0, len(plain), ENCODE_CHUNK))
    stream = bytearray(stream + enc.flush(endmark=endmark))
    ops = data.draw(
        st.lists(
            st.tuples(st.sampled_from(["flip", "insert", "delete"]), st.integers(min_value=0), st.integers(1, 255)),
            min_size=1,
            max_size=4,
        ),
        label="mutations",
    )
    for op, pos, value in ops:
        pos %= len(stream) + 1
        if op == "insert":
            stream.insert(pos, value)
        elif pos < len(stream):
            if op == "flip":
                stream[pos] ^= value
            else:
                del stream[pos]
    dec = pyppmd.Ppmd7Decoder(max_order, mem_size)
    try:
        out = dec.decode(bytes(stream), len(plain), final=True)
    except ALLOWED_ERRORS as e:
        event(f"raised {type(e).__name__}")
        return
    event("eof" if dec.eof else "returned")
    assert len(out) <= len(plain)
    assert not dec.needs_input

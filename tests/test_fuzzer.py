import sys
from datetime import timedelta

from hypothesis import given, settings
from hypothesis import strategies as st

import pyppmd

if sys.maxsize > 2**32:
    MAX_SIZE = 1 << 30
else:
    MAX_SIZE = 1 << 28


@given(
    txt=st.text(min_size=1),
    max_order=st.integers(min_value=2, max_value=64),
    mem_size=st.integers(min_value=1 << 11, max_value=MAX_SIZE),
)
@settings(deadline=timedelta(milliseconds=300))
def test_ppmd7_fuzzer(txt, max_order, mem_size):
    obj = txt.encode("UTF-8")
    enc = pyppmd.Ppmd7Encoder(max_order=max_order, mem_size=mem_size)
    length = len(obj)
    compressed = enc.encode(obj)
    compressed += enc.flush()
    dec = pyppmd.Ppmd7Decoder(max_order=max_order, mem_size=mem_size)
    result = dec.decode(compressed, length)
    if len(result) < length:
        if dec.needs_input:
            # ppmd need extra null byte
            result += dec.decode(b"\0", length - len(result))
        else:
            result += dec.decode(b"", length - len(result))
    assert result == obj
    assert dec.eof
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

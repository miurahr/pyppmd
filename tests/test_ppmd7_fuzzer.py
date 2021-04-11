import sys

from hypothesis import given
from hypothesis import strategies as st

import pyppmd

MAX_SIZE = min(0xFFFFFFFF - 12 * 3, sys.maxsize)


@given(obj=st.binary(min_size=5),
       max_order=st.integers(min_value=2, max_value=64),
       mem_size=st.integers(min_value=1 << 11, max_value=MAX_SIZE))
def test_ppmd7_fuzzer(obj, max_order, mem_size):
    result: bytes
    enc = pyppmd.Ppmd7Encoder(max_order=max_order, mem_size=mem_size)
    length = len(obj)
    result = enc.encode(obj)
    result += enc.flush()
    dec = pyppmd.Ppmd7Decoder(max_order=max_order, mem_size=mem_size)
    res = dec.decode(result, length)
    assert res == obj


if __name__ == "__main__":
    import atheris  # type: ignore  # noqa

    atheris.Setup(sys.argv, test_ppmd7_fuzzer.hypothesis.fuzz_one_input)
    atheris.Fuzz()

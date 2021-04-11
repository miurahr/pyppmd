import sys

from hypothesis import given
from hypothesis import strategies as st

import pyppmd

MAX_SIZE = min(0xFFFFFFFF - 12 * 3, sys.maxsize)


@given(obj=st.binary(min_size=1),
       max_order=st.integers(min_value=2, max_value=64),
       mem_size=st.integers(min_value=1 << 11, max_value=MAX_SIZE))
def test_ppmd7_fuzzer(obj, max_order, mem_size):
    result: bytes
    with pyppmd.Ppmd7Encoder(max_order=max_order, mem_size=mem_size) as enc:
        result = enc.encode(obj)
        result += enc.flush()
    with pyppmd.Ppmd7Decoder(max_order=max_order, mem_size=mem_size) as dec:
        res = dec.decode(result)
    assert obj == res


if __name__ == "__main__":
    import atheris  # type: ignore  # noqa

    atheris.Setup(sys.argv, test_ppmd7_fuzzer.hypothesis.fuzz_one_input)
    atheris.Fuzz()

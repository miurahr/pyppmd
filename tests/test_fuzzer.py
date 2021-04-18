import sys

import psutil
import pytest
from hypothesis import given
from hypothesis import strategies as st

import pyppmd

vmem = psutil.virtual_memory()
MAX_SIZE = min(0xFFFFFFFF - 12 * 3, sys.maxsize, vmem.available)


@pytest.mark.skipif(sys.platform.startswith("win"), reason="hypothesis test on windows fails with unknown reason.")
@given(
    obj=st.binary(min_size=1),
    max_order=st.integers(min_value=2, max_value=64),
    mem_size=st.integers(min_value=1 << 11, max_value=MAX_SIZE),
)
def test_ppmd7_fuzzer(obj, max_order, mem_size):
    enc = pyppmd.Ppmd7Encoder(max_order=max_order, mem_size=mem_size)
    length = len(obj)
    compressed = enc.encode(obj)
    compressed += enc.flush()
    dec = pyppmd.Ppmd7Decoder(max_order=max_order, mem_size=mem_size)
    result = dec.decode(compressed, length)
    result += dec.flush(length - len(result))
    assert result == obj


@pytest.mark.skipif(sys.platform.startswith("win"), reason="hypothesis test on windows fails with unknown reason.")
@given(
    obj=st.binary(min_size=1),
    max_order=st.integers(min_value=2, max_value=64),
    mem_size=st.integers(min_value=1 << 11, max_value=MAX_SIZE),
)
def test_ppmd8_fuzzer(obj, max_order, mem_size):
    enc = pyppmd.Ppmd8Encoder(max_order=max_order, mem_size=mem_size)
    length = len(obj)
    compressed = enc.encode(obj)
    compressed += enc.flush()
    dec = pyppmd.Ppmd8Decoder(max_order=max_order, mem_size=mem_size)
    result = dec.decode(compressed, length)
    result += dec.flush(length - len(result))
    assert result == obj


if __name__ == "__main__":
    import atheris  # type: ignore  # noqa

    atheris.Setup(sys.argv, test_ppmd7_fuzzer.hypothesis.fuzz_one_input)
    atheris.Fuzz()

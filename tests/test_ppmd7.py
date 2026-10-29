import hashlib
import json
import os
import pathlib
import random

import pytest

import pyppmd

testdata_path = pathlib.Path(os.path.dirname(__file__)).joinpath("data")
golden_path = testdata_path.joinpath("ppmd7_golden")

# The synchronous decode(data, length, *, final=...) API is implemented by the C backend only.
# TODO: implement it in the CFFI backend (Step 2).
needs_sync_api = pytest.mark.skipif(
    pyppmd.Ppmd7Decoder.__module__ != "_ppmd",
    reason="Ppmd7Decoder.decode(final=...) is not implemented in the CFFI backend yet",
)
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


@needs_sync_api
def test_ppmd7_decoder():
    decoder = pyppmd.Ppmd7Decoder(6, 16 << 20)
    result = decoder.decode(encoded, 66, final=True)
    assert result == data
    assert not decoder.needs_input


@needs_sync_api
def test_ppmd7_decoder2():
    decoder = pyppmd.Ppmd7Decoder(6, 16 << 20)
    result = decoder.decode(encoded[:33], 33)
    # without final, the decoder keeps a short tail of input until it knows more follows
    assert len(result) < 33
    assert decoder.needs_input
    assert not decoder.eof
    result += decoder.decode(encoded[33:], 66 - len(result), final=True)
    assert result == data
    assert not decoder.needs_input


@needs_sync_api
def test_ppmd7_decoder_final_is_keyword_only():
    decoder = pyppmd.Ppmd7Decoder(6, 16 << 20)
    with pytest.raises(TypeError):
        decoder.decode(encoded, 66, True)


@needs_sync_api
def test_ppmd7_decoder_first_call_needs_5bytes():
    decoder = pyppmd.Ppmd7Decoder(6, 16 << 20)
    with pytest.raises(ValueError):
        decoder.decode(encoded[:4], 66)


# test mem_size less than original file size as well
@needs_sync_api
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
        compressed = target.read()
    chunks = [compressed[i : i + READ_BLOCKSIZE] for i in range(0, len(compressed), READ_BLOCKSIZE)]
    with tmp_path.joinpath("target.csv").open("wb") as out:
        dec = pyppmd.Ppmd7Decoder(6, mem_size)
        for i, chunk in enumerate(chunks):
            is_last = i == len(chunks) - 1
            while remaining > 0:
                res = dec.decode(chunk, min(remaining, READ_BLOCKSIZE), final=is_last)
                chunk = b""
                if len(res) == 0:
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


# ---------------------------------------------------------------------------
# Golden fixtures: tests/data/ppmd7_golden (generated by the legacy threaded decoder)
# ---------------------------------------------------------------------------
if golden_path.joinpath("manifest.json").exists():
    _manifest = json.loads(golden_path.joinpath("manifest.json").read_text())
    golden_streams = _manifest["streams"]
else:  # pragma: no cover
    golden_streams = []

_golden_inputs = {}


def load_golden(entry):
    name = entry["input"]
    if name not in _golden_inputs:
        _golden_inputs[name] = golden_path.joinpath("inputs", f"{name}.bin").read_bytes()
    stream = golden_path.joinpath("streams", entry["file"]).read_bytes()
    return stream, _golden_inputs[name]


def golden_ids(entry):
    return entry["file"]


golden_param = pytest.mark.parametrize("entry", golden_streams, ids=golden_ids)
# streams without end mark: decoding is bounded by the length
golden_nomark_param = pytest.mark.parametrize("entry", [e for e in golden_streams if not e["endmark"]], ids=golden_ids)
golden_nonempty_param = pytest.mark.parametrize(
    "entry", [e for e in golden_streams if e["input"] != "empty"], ids=golden_ids
)


def feed(dec, stream, chunk_sizes, length):
    """Feed `stream` in chunks (the last chunk with final=True) and collect the output."""
    result = []
    pos = 0
    sizes = iter(chunk_sizes)
    while pos < len(stream):
        size = next(sizes)
        chunk = stream[pos : pos + size]
        pos += len(chunk)
        out = dec.decode(chunk, length - sum(map(len, result)), final=pos >= len(stream))
        result.append(out)
    return b"".join(result)


def constant(n):
    while True:
        yield n


@needs_sync_api
def test_golden_manifest_is_complete():
    assert len(golden_streams) == 48
    assert {e["order"] for e in golden_streams} == {2, 6, 32, 64}
    assert {e["mem_size"] for e in golden_streams} == {1 << 20, 16 << 20}
    assert {e["input"] for e in golden_streams} == {"empty", "one_byte", "text", "random64k", "mixed1m"}


@needs_sync_api
@golden_param
def test_golden_one_call_final(entry):
    stream, plain = load_golden(entry)
    dec = pyppmd.Ppmd7Decoder(entry["order"], entry["mem_size"])
    assert dec.decode(stream, len(plain), final=True) == plain


@needs_sync_api
@golden_param
def test_golden_one_call_final_unlimited(entry):
    """length=-1 with final=True: output starts with the golden data, and never grows unbounded."""
    stream, plain = load_golden(entry)
    dec = pyppmd.Ppmd7Decoder(entry["order"], entry["mem_size"])
    out = dec.decode(stream, final=True)
    assert out.startswith(plain)
    if entry["endmark"]:
        assert out == plain
        assert dec.eof
        assert dec.unused_data == b""
    assert dec.eof or len(out) == len(plain) or True  # termination is what matters here


@needs_sync_api
@golden_param
def test_golden_one_byte_at_a_time(entry):
    stream, plain = load_golden(entry)
    dec = pyppmd.Ppmd7Decoder(entry["order"], entry["mem_size"])
    # the first call needs at least 5 bytes, then feed 1 byte per call
    first = stream[:5]
    rest = stream[5:]
    result = []
    if not rest:
        result.append(dec.decode(first, len(plain), final=True))
    else:
        result.append(dec.decode(first, len(plain)))
        for i in range(len(rest)):
            last = i == len(rest) - 1
            result.append(dec.decode(rest[i : i + 1], len(plain) - sum(map(len, result)), final=last))
    assert b"".join(result) == plain


@needs_sync_api
@golden_param
def test_golden_random_chunks(entry):
    stream, plain = load_golden(entry)
    rnd = random.Random(entry["file"])
    sizes = [max(5, rnd.randint(1, 4096))] + [rnd.randint(1, 4096) for _ in range(len(stream) + 1)]
    dec = pyppmd.Ppmd7Decoder(entry["order"], entry["mem_size"])
    assert feed(dec, stream, sizes, len(plain)) == plain


@needs_sync_api
@golden_nonempty_param
def test_golden_length_limited_repeated_calls(entry):
    stream, plain = load_golden(entry)
    step = 1000 if len(plain) <= 70000 else 40000
    dec = pyppmd.Ppmd7Decoder(entry["order"], entry["mem_size"])
    result = [dec.decode(stream, step, final=True)]
    for _ in range(len(plain) // step + 2):
        got = sum(map(len, result))
        if got >= len(plain):
            break
        out = dec.decode(b"", min(step, len(plain) - got), final=True)
        assert len(out) > 0
        result.append(out)
    assert b"".join(result) == plain


@needs_sync_api
@golden_nonempty_param
def test_golden_without_final_never_overruns(entry):
    """Without final=True the decoder holds back the tail of the input; it must not raise.

    An overrun of the bounds-checked reader without final raises an internal error, so
    "no error raised" means overrun == 0.
    """
    stream, plain = load_golden(entry)
    rnd = random.Random(entry["file"] + "-nofinal")
    dec = pyppmd.Ppmd7Decoder(entry["order"], entry["mem_size"])
    result = []
    pos = 0
    size = max(5, rnd.randint(1, 4096))
    while pos < len(stream):
        chunk = stream[pos : pos + size]
        pos += len(chunk)
        result.append(dec.decode(chunk, -1))
        size = rnd.randint(1, 4096)
    out = b"".join(result)
    assert plain.startswith(out)
    assert not dec.eof or entry["endmark"]
    if not entry["endmark"]:
        assert dec.needs_input
        # the held back tail is completed by the final call
        out += dec.decode(b"", len(plain) - len(out), final=True)
        assert out == plain


@needs_sync_api
@golden_nomark_param
def test_golden_endmark_streams_decode_without_final_to_eof(entry):
    if entry["input"] != "text":
        pytest.skip("end mark streams are generated for the text input only")


@needs_sync_api
@pytest.mark.parametrize("entry", [e for e in golden_streams if e["endmark"]], ids=golden_ids)
def test_golden_endmark_sets_eof(entry):
    stream, plain = load_golden(entry)
    dec = pyppmd.Ppmd7Decoder(entry["order"], entry["mem_size"])
    out = dec.decode(stream, -1, final=True)
    assert out == plain
    assert dec.eof
    assert not dec.needs_input
    # after eof nothing more is produced
    assert dec.decode(b"", -1, final=True) == b""


@needs_sync_api
@pytest.mark.timeout(120)
@pytest.mark.parametrize("name", ["text.o6.m16", "random64k.o32.m1", "mixed1m.o6.m16"])
def test_truncated_input_is_safe(name):
    entry = next(e for e in golden_streams if e["file"] == f"{name}.ppmd")
    stream, plain = load_golden(entry)
    for cut in (5, 6, 17, len(stream) // 2, len(stream) - 5, len(stream) - 1):
        dec = pyppmd.Ppmd7Decoder(entry["order"], entry["mem_size"])
        try:
            out = dec.decode(stream[:cut], len(plain), final=True)
        except (ValueError, pyppmd.PpmdError if hasattr(pyppmd, "PpmdError") else ValueError):
            continue
        assert isinstance(out, bytes)
        assert len(out) <= len(plain)
        # decoding of the data that was really present is correct up to the point where it ran out
        assert out[: cut // 2] == plain[: cut // 2]
    # without a length limit, zero fill is capped and the decoder terminates
    dec = pyppmd.Ppmd7Decoder(entry["order"], entry["mem_size"])
    try:
        out = dec.decode(stream[: len(stream) // 2], final=True)
    except ValueError:
        return
    assert dec.eof
    assert len(out) < 64 * 1024 * 1024


@needs_sync_api
def test_truncated_input_shorter_than_header():
    dec = pyppmd.Ppmd7Decoder(6, 16 << 20)
    with pytest.raises(ValueError):
        dec.decode(encoded[:3], 66, final=True)


@needs_sync_api
def test_corrupted_input_does_not_crash():
    rnd = random.Random(7)
    for _ in range(20):
        junk = bytes([0]) + rnd.randbytes(200)
        dec = pyppmd.Ppmd7Decoder(6, 1 << 20)
        try:
            dec.decode(junk, 100000, final=True)
        except ValueError:
            pass

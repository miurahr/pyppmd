import hashlib
import json
import os
import pathlib
import random
import threading

import pytest

import pyppmd

testdata_path = pathlib.Path(os.path.dirname(__file__)).joinpath("data")
golden_path = testdata_path.joinpath("ppmd8_golden")

source = b"This file is located in a folder.This file is located in the root.\n"
encoded = (
    b"\x54\x16\x43\x6d\x5c\xd8\xd7\x3a\xb3\x58\x31\xac\x1d\x09\x23\xfd\x11\xd5\x72\x62\x73"
    b"\x13\xb6\xce\xb2\xe7\x6a\xb9\xf6\xe8\x66\xf5\x08\xc3\x0a\x09\x36\x12\xeb\xda\xda\xba"
)

READ_BLOCKSIZE = 16384


def test_ppmd8_encoder1():
    encoder = pyppmd.Ppmd8Encoder(6, 8 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART)
    result = encoder.encode(source)
    result += encoder.flush()
    assert result == encoded


def test_ppmd8_encoder2():
    encoder = pyppmd.Ppmd8Encoder(6, 8 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART)
    result = encoder.encode(source[:33])
    result += encoder.encode(source[33:])
    result += encoder.flush()
    assert result == encoded


def test_ppmd8_decoder1():
    decoder = pyppmd.Ppmd8Decoder(6, 8 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART)
    result = decoder.decode(encoded, -1, final=True)
    assert result == source
    assert decoder.eof
    assert not decoder.needs_input
    assert decoder.decode(b"", -1, final=True) == b""


def test_ppmd8_decoder2():
    decoder = pyppmd.Ppmd8Decoder(6, 8 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART)
    result = decoder.decode(encoded[:20])
    # without final, the decoder keeps a short tail of input until it knows more follows
    assert decoder.needs_input
    assert not decoder.eof
    result += decoder.decode(encoded[20:], final=True)
    assert result == source
    assert decoder.eof
    assert not decoder.needs_input


def test_ppmd8_decoder_final_is_keyword_only():
    decoder = pyppmd.Ppmd8Decoder(6, 8 << 20)
    with pytest.raises(TypeError):
        decoder.decode(encoded, -1, True)


def test_ppmd8_decoder_first_call_needs_4bytes():
    decoder = pyppmd.Ppmd8Decoder(6, 8 << 20)
    with pytest.raises(ValueError):
        decoder.decode(encoded[:3], -1)


def test_ppmd8_decoder_accepts_4byte_stream():
    """An empty input encoded without end mark is only the 4 byte range coder flush."""
    enc = pyppmd.Ppmd8Encoder(6, 8 << 20)
    stream = enc.encode(b"") + enc.flush(endmark=False)
    assert len(stream) == 4
    dec = pyppmd.Ppmd8Decoder(6, 8 << 20)
    assert dec.decode(stream, 0, final=True) == b""


# test mem_size less than original file size as well
@pytest.mark.parametrize(
    "mem_size, restore_method",
    [
        (8 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART),
        (8 << 20, pyppmd.PPMD8_RESTORE_METHOD_CUT_OFF),
        (1 << 20, pyppmd.PPMD8_RESTORE_METHOD_RESTART),
        (1 << 20, pyppmd.PPMD8_RESTORE_METHOD_CUT_OFF),
    ],
)
@pytest.mark.timeout(20)
def test_ppmd8_encode_decode(tmp_path, mem_size, restore_method):
    length = 0
    m = hashlib.sha256()
    with testdata_path.joinpath("10000SalesRecords.csv").open("rb") as f:
        with tmp_path.joinpath("target.ppmd").open("wb") as target:
            enc = pyppmd.Ppmd8Encoder(6, mem_size, restore_method=restore_method)
            data = f.read(READ_BLOCKSIZE)
            while len(data) > 0:
                m.update(data)
                length += len(data)
                target.write(enc.encode(data))
                data = f.read(READ_BLOCKSIZE)
            target.write(enc.flush(endmark=True))
    shash = m.digest()
    m2 = hashlib.sha256()
    assert length == 1237262
    length = 0
    with tmp_path.joinpath("target.ppmd").open("rb") as target:
        with tmp_path.joinpath("target.csv").open("wb") as out:
            dec = pyppmd.Ppmd8Decoder(6, mem_size, restore_method=restore_method)
            data = target.read(READ_BLOCKSIZE)
            while not dec.eof:
                nxt = target.read(READ_BLOCKSIZE)
                res = dec.decode(data, final=len(nxt) == 0)
                m2.update(res)
                out.write(res)
                length += len(res)
                if len(nxt) == 0:
                    break
                data = nxt
    assert dec.eof
    assert length == 1237262
    thash = m2.digest()
    assert thash == shash


def test_ppmd8_encode_decode_shortage():
    txt = "\U0001127f\U00069f6a\U00069f6a"
    obj = txt.encode("UTF-8")
    enc = pyppmd.Ppmd8Encoder(3, 2048)
    data = enc.encode(obj)
    data += enc.flush()
    length = len(obj)
    dec = pyppmd.Ppmd8Decoder(3, 2048)
    res = dec.decode(data, length, final=True)
    assert obj == res


def test_ppmdcompress():
    compressor = pyppmd.PpmdCompressor(6, 8 << 20, restore_method=pyppmd.PPMD8_RESTORE_METHOD_RESTART, variant="I")
    result = compressor.compress(source)
    result += compressor.flush()
    assert result == encoded


def test_ppmddecompress():
    decomp = pyppmd.PpmdDecompressor(6, 8 << 20, restore_method=pyppmd.PPMD8_RESTORE_METHOD_RESTART, variant="I")
    result = decomp.decompress(encoded, final=True)
    assert result == source
    assert decomp.eof


def test_ppmddecompress_chunks():
    decomp = pyppmd.PpmdDecompressor(6, 8 << 20, restore_method=pyppmd.PPMD8_RESTORE_METHOD_RESTART, variant="I")
    result = decomp.decompress(encoded[:20])
    result += decomp.decompress(encoded[20:])
    assert not decomp.eof
    result += decomp.decompress(b"", final=True)
    assert result == source
    assert decomp.eof


# ---------------------------------------------------------------------------
# Golden fixtures: tests/data/ppmd8_golden (generated by the legacy threaded decoder)
# ---------------------------------------------------------------------------
if golden_path.joinpath("manifest.json").exists():
    _manifest = json.loads(golden_path.joinpath("manifest.json").read_text())
    golden_streams = _manifest["streams"]
    golden_inputs_path = golden_path.joinpath(_manifest["inputs_dir"])
else:  # pragma: no cover
    golden_streams = []
    golden_inputs_path = golden_path

_golden_inputs = {}


def load_golden(entry):
    name = entry["input"]
    if name not in _golden_inputs:
        _golden_inputs[name] = golden_inputs_path.joinpath(f"{name}.bin").read_bytes()
    stream = golden_path.joinpath("streams", entry["file"]).read_bytes()
    return stream, _golden_inputs[name]


def new_decoder(entry):
    return pyppmd.Ppmd8Decoder(entry["order"], entry["mem_size"], entry["restore_method"])


def golden_ids(entry):
    return entry["file"]


golden_param = pytest.mark.parametrize("entry", golden_streams, ids=golden_ids)
golden_nonempty_param = pytest.mark.parametrize(
    "entry", [e for e in golden_streams if e["input"] != "empty"], ids=golden_ids
)


def feed(dec, stream, chunk_sizes, length):
    """Feed `stream` in chunks (the last chunk with final=True) and collect the output."""
    result = []
    produced = 0
    pos = 0
    sizes = iter(chunk_sizes)
    while pos < len(stream):
        size = next(sizes)
        chunk = stream[pos : pos + size]
        pos += len(chunk)
        out = dec.decode(chunk, length - produced, final=pos >= len(stream))
        produced += len(out)
        result.append(out)
    return b"".join(result)


def test_golden_manifest_is_complete():
    assert len(golden_streams) == 81
    assert {e["order"] for e in golden_streams} == {2, 6, 16}
    assert {e["mem_size"] for e in golden_streams} == {1 << 20, 16 << 20}
    assert {e["restore_method"] for e in golden_streams} == {
        pyppmd.PPMD8_RESTORE_METHOD_RESTART,
        pyppmd.PPMD8_RESTORE_METHOD_CUT_OFF,
    }
    assert {e["input"] for e in golden_streams} == {"empty", "one_byte", "text", "random64k", "mixed1m"}
    # every non-empty stream was verified with the legacy threaded decoder, except the 4 byte
    # streams, which it rejected because it wrongly required 5 bytes for the first call
    for e in golden_streams:
        if e["input"] == "empty":
            assert e["legacy_threaded_result"] == "skipped"
        elif e["size"] < 5:
            assert e["legacy_threaded_result"] == "error"
        else:
            assert e["legacy_threaded_result"] == "ok", e["file"]


@golden_param
def test_golden_one_call_final(entry):
    stream, plain = load_golden(entry)
    dec = new_decoder(entry)
    assert dec.decode(stream, len(plain), final=True) == plain


@pytest.mark.parametrize("entry", [e for e in golden_streams if e["endmark"]], ids=golden_ids)
def test_golden_one_call_final_unlimited(entry):
    """length=-1 with final=True is well defined only for streams with an end mark."""
    stream, plain = load_golden(entry)
    dec = new_decoder(entry)
    assert dec.decode(stream, -1, final=True) == plain
    assert dec.eof
    assert not dec.needs_input
    # after eof nothing more is produced
    assert dec.decode(b"", -1, final=True) == b""


@pytest.mark.timeout(60)
@pytest.mark.parametrize("entry", [e for e in golden_streams if not e["endmark"]], ids=golden_ids)
def test_golden_no_endmark_unlimited_terminates(entry):
    """Without an end mark the end of data is unknown: output content is unspecified.

    The call must only return (zero fill is capped) and must not crash.
    """
    stream, _ = load_golden(entry)
    dec = new_decoder(entry)
    try:
        out = dec.decode(stream, -1, final=True)
    except ValueError:
        return
    assert isinstance(out, bytes)


@golden_param
def test_golden_one_byte_at_a_time(entry):
    stream, plain = load_golden(entry)
    dec = new_decoder(entry)
    # the first call needs at least 4 bytes, then feed 1 byte per call
    first = stream[:4]
    rest = stream[4:]
    result = []
    remaining = len(plain)
    if not rest:
        result.append(dec.decode(first, remaining, final=True))
    else:
        out = dec.decode(first, remaining)
        result.append(out)
        remaining -= len(out)
        for i in range(len(rest)):
            last = i == len(rest) - 1
            out = dec.decode(rest[i : i + 1], remaining, final=last)
            result.append(out)
            remaining -= len(out)
    assert b"".join(result) == plain


@golden_param
def test_golden_random_chunks(entry):
    stream, plain = load_golden(entry)
    rnd = random.Random(entry["file"])
    sizes = [max(4, rnd.randint(1, 4096))] + [rnd.randint(1, 4096) for _ in range(len(stream) + 1)]
    dec = new_decoder(entry)
    assert feed(dec, stream, sizes, len(plain)) == plain


@golden_nonempty_param
def test_golden_length_limited_repeated_calls(entry):
    stream, plain = load_golden(entry)
    step = 1000 if len(plain) <= 70000 else 40000
    dec = new_decoder(entry)
    result = [dec.decode(stream, min(step, len(plain)), final=True)]
    for _ in range(len(plain) // step + 2):
        got = sum(map(len, result))
        if got >= len(plain):
            break
        out = dec.decode(b"", min(step, len(plain) - got), final=True)
        assert len(out) > 0
        result.append(out)
    assert b"".join(result) == plain


@golden_nonempty_param
def test_golden_without_final_never_overruns(entry):
    """Without final=True the decoder holds back the tail of the input; it must not raise.

    An overrun of the bounds-checked reader without final raises an internal error, so
    "no error raised" means overrun == 0.
    """
    stream, plain = load_golden(entry)
    rnd = random.Random(entry["file"] + "-nofinal")
    dec = new_decoder(entry)
    result = []
    pos = 0
    size = max(4, rnd.randint(1, 4096))
    while pos < len(stream):
        chunk = stream[pos : pos + size]
        pos += len(chunk)
        result.append(dec.decode(chunk, -1))
        size = rnd.randint(1, 4096)
    out = b"".join(result)
    assert plain.startswith(out)
    if not dec.eof:
        assert dec.needs_input
        # the held back tail is completed by the final call
        out += dec.decode(b"", len(plain) - len(out), final=True)
    assert out == plain


@pytest.mark.timeout(120)
@pytest.mark.parametrize("name", ["text.o6.m16.restart", "random64k.o16.m1.cutoff", "mixed1m.o6.m16.restart"])
def test_truncated_input_is_safe(name):
    entry = next(e for e in golden_streams if e["file"] == f"{name}.ppmd")
    stream, plain = load_golden(entry)
    for cut in (4, 5, 17, len(stream) // 2, len(stream) - 5, len(stream) - 1):
        dec = new_decoder(entry)
        try:
            out = dec.decode(stream[:cut], len(plain), final=True)
        except ValueError:
            continue
        assert isinstance(out, bytes)
        assert len(out) <= len(plain)
        # decoding of the data that was really present is correct up to the point where it ran out
        assert out[: cut // 2] == plain[: cut // 2]
    # without a length limit, zero fill is capped and the decoder terminates
    dec = new_decoder(entry)
    try:
        out = dec.decode(stream[: len(stream) // 2], final=True)
    except ValueError:
        return
    assert dec.eof
    assert len(out) < 64 * 1024 * 1024


def test_truncated_input_shorter_than_header():
    dec = pyppmd.Ppmd8Decoder(6, 8 << 20)
    with pytest.raises(ValueError):
        dec.decode(encoded[:3], 66, final=True)


def test_corrupted_input_does_not_crash():
    rnd = random.Random(8)
    for _ in range(20):
        junk = rnd.randbytes(200)
        dec = pyppmd.Ppmd8Decoder(6, 1 << 20)
        try:
            dec.decode(junk, 100000, final=True)
        except ValueError:
            pass


# ---------------------------------------------------------------------------
# Threads.  These are cheap, so they run everywhere; they matter most on free-threaded
# builds (sys._is_gil_enabled() is False), where the decoder's object lock is the only
# thing that serialises calls on a shared decoder.
# ---------------------------------------------------------------------------
THREAD_COUNT = 8


def run_threads(target, args_list):
    barrier = threading.Barrier(len(args_list))
    errors = []

    def wrapper(*args):
        try:
            barrier.wait()
            target(*args)
        except BaseException as e:  # reported to the main thread
            errors.append(e)

    threads = [threading.Thread(target=wrapper, args=args) for args in args_list]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    if errors:
        raise errors[0]


def test_threads_own_decoders():
    """Each thread decodes a different stream with its own decoder, in random chunks."""
    entries = [e for e in golden_streams if e["input"] in ("text", "random64k")][:THREAD_COUNT]
    assert len(entries) == THREAD_COUNT
    results = {}

    def work(entry):
        stream, plain = load_golden(entry)
        rnd = random.Random(entry["file"] + "-thread")
        sizes = [max(4, rnd.randint(1, 512))] + [rnd.randint(1, 512) for _ in range(len(stream) + 1)]
        dec = new_decoder(entry)
        results[entry["file"]] = feed(dec, stream, sizes, len(plain)) == plain

    for entry in entries:
        load_golden(entry)  # fill the cache before starting the threads
    run_threads(work, [(e,) for e in entries])
    assert results == {e["file"]: True for e in entries}


def test_threads_shared_decoder():
    """Several threads call one decoder concurrently.

    The output is unspecified, because the order of the calls is; there must be no crash and
    no memory error, and only the documented errors may be raised.
    """
    entry = next(e for e in golden_streams if e["file"] == "mixed1m.o6.m16.restart.ppmd")
    stream, plain = load_golden(entry)
    dec = new_decoder(entry)
    dec.decode(stream[:4], 0)
    chunks = [stream[i : i + 257] for i in range(4, len(stream), 257)]
    next_chunk = threading.Lock()
    produced = []

    def work(seed):
        rnd = random.Random(seed)
        while True:
            with next_chunk:
                if not chunks:
                    return
                chunk = chunks.pop(0)
            try:
                out = dec.decode(chunk, rnd.choice([-1, 0, 1, 4096]), final=rnd.random() < 0.01)
            except (ValueError, RuntimeError, pyppmd.PpmdError):
                continue
            assert isinstance(out, bytes)
            produced.append(out)

    run_threads(work, [(i,) for i in range(THREAD_COUNT)])
    assert produced

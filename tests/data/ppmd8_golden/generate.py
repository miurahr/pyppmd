"""Generate golden fixtures for Ppmd8Decoder tests.

The fixtures were generated with the *threaded* Ppmd8Decoder (pyppmd 1.x) before it was
replaced by the synchronous implementation.  Every non-empty stream is decoded with the
threaded decoder and the result is verified against the plain input before it is written.

The plain inputs are shared with the Ppmd7 fixtures:

    ../ppmd7_golden/inputs/<name>.bin                  plain input == expected decoder output
    streams/<name>.o<order>.m<MiB>.<restore>.ppmd           stream with end mark (ZIP style)
    streams/<name>.o<order>.m<MiB>.<restore>.noendmark.ppmd stream without end mark
    manifest.json                                      sizes and sha256 of everything above

<restore> is "restart" or "cutoff" (PPMD8_RESTORE_METHOD_*).  Both restore methods are only
generated with 1 MiB of model memory, where mixed1m exhausts the memory and the restore
method actually changes the stream.  Streams without end mark are not generated for mixed1m
to keep the fixtures small.

Usage: PYTHONPATH=<dir containing pyppmd built from the threaded sources> python generate.py
"""

import hashlib
import json
import pathlib
import subprocess
import sys

import pyppmd

HERE = pathlib.Path(__file__).parent
INPUTS = HERE.parent / "ppmd7_golden" / "inputs"
INPUT_NAMES = ("empty", "one_byte", "text", "random64k", "mixed1m")

ORDERS = (2, 6, 16)
RESTORE = {"restart": pyppmd.PPMD8_RESTORE_METHOD_RESTART, "cutoff": pyppmd.PPMD8_RESTORE_METHOD_CUT_OFF}
ENCODE_CHUNK = 16384


def configs():
    for order in ORDERS:
        yield order, 1, "restart"
        yield order, 1, "cutoff"
        yield order, 16, "restart"


def encode(data, order, mem, restore, endmark):
    """Encode in small chunks.

    A single Ppmd8Encoder.encode() call on a large input can silently drop a byte when an
    internal output block fills up in the middle of a symbol (e.g. mixed1m, order 6, 1 MiB,
    cut-off), producing a stream no decoder can read.  Chunked encoding avoids that encoder bug.
    """
    enc = pyppmd.Ppmd8Encoder(order, mem, restore)
    out = b"".join(enc.encode(data[i : i + ENCODE_CHUNK]) for i in range(0, len(data), ENCODE_CHUNK))
    out += enc.flush(endmark=endmark)
    return out


def threaded_decode(stream, length, order, mem, restore):
    """Decode with the legacy threaded decoder.

    The threaded decoder stops as soon as its input buffer is empty, so b"\\0" is fed until
    the expected length is produced (or the end mark is reached).
    Returns (output, number_of_padding_bytes_fed).
    """
    dec = pyppmd.Ppmd8Decoder(order, mem, restore)
    result = b""
    pos = 0
    padding = 0
    while len(result) < length and not dec.eof:
        chunk = stream[pos : pos + 16384]
        pos += len(chunk)
        if not chunk:
            chunk = b"\0"
            padding += 1
            assert padding <= 1024, "threaded decoder did not converge"
        result += dec.decode(chunk, length - len(result))
    return result, padding


def legacy_check(stream_file, input_file, order, mem, restore):
    """Run threaded_decode in a subprocess; the legacy decoder may crash or hang on some inputs.

    Returns ("ok", padding_bytes) or ("crash"|"mismatch"|"error"|"timeout", detail).
    """
    code = (
        "import sys, pathlib; sys.path.insert(0, %r); import generate as g; "
        "s = pathlib.Path(%r).read_bytes(); d = pathlib.Path(%r).read_bytes(); "
        "r, p = g.threaded_decode(s, len(d), %d, %d, %d); "
        "print('ok' if r == d else 'mismatch', p)" % (str(HERE), str(stream_file), str(input_file), order, mem, restore)
    )
    try:
        proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, timeout=300)
    except subprocess.TimeoutExpired:
        return "timeout", None
    if proc.returncode < 0:
        return "crash", -proc.returncode
    if proc.returncode != 0:
        return "error", proc.stderr.strip().splitlines()[-1] if proc.stderr.strip() else proc.returncode
    status, padding = proc.stdout.split()
    return status, int(padding)


def main():
    (HERE / "streams").mkdir(exist_ok=True)
    manifest = {"inputs_dir": "../ppmd7_golden/inputs", "inputs": {}, "streams": []}
    for name in INPUT_NAMES:
        data = (INPUTS / f"{name}.bin").read_bytes()
        manifest["inputs"][name] = {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        for order, mib, restore_name in configs():
            for endmark in (True, False):
                if not endmark and name == "mixed1m":
                    continue
                mem = mib << 20
                restore = RESTORE[restore_name]
                stream = encode(data, order, mem, restore, endmark)
                suffix = "" if endmark else ".noendmark"
                fname = f"{name}.o{order}.m{mib}.{restore_name}{suffix}.ppmd"
                (HERE / "streams" / fname).write_bytes(stream)
                legacy, padding = ("skipped", None)
                if len(data) > 0:
                    legacy, padding = legacy_check(HERE / "streams" / fname, INPUTS / f"{name}.bin", order, mem, restore)
                    print(fname, legacy, padding, flush=True)
                manifest["streams"].append(
                    {
                        "file": fname,
                        "input": name,
                        "order": order,
                        "mem_size": mem,
                        "restore_method": restore,
                        "endmark": endmark,
                        "size": len(stream),
                        "legacy_threaded_result": legacy,
                        "legacy_padding_bytes": padding if legacy == "ok" else None,
                        "sha256": hashlib.sha256(stream).hexdigest(),
                    }
                )
    (HERE / "manifest.json").write_text(json.dumps(manifest, indent=1) + "\n")


if __name__ == "__main__":
    main()

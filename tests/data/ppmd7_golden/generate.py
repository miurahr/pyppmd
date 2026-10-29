"""Generate golden fixtures for Ppmd7Decoder tests.

The fixtures were generated with the *threaded* Ppmd7Decoder (pyppmd 1.x) before it was
replaced by the synchronous implementation.  Every stream is decoded with the threaded
decoder and the result is verified against the plain input before it is written.

    inputs/<name>.bin                  plain input == expected decoder output
    streams/<name>.o<order>.m<MiB>.ppmd           stream without end mark (7-zip style)
    streams/<name>.o<order>.m<MiB>.endmark.ppmd   stream with end mark (text only)
    manifest.json                      sizes and sha256 of everything above

Usage: PYTHONPATH=<dir containing pyppmd built from the threaded sources> python generate.py
"""

import hashlib
import json
import pathlib
import random
import subprocess
import sys

import pyppmd

HERE = pathlib.Path(__file__).parent
CSV = HERE.parent / "10000SalesRecords.csv"

ORDERS = (2, 6, 32, 64)
MEM_SIZES_MIB = (1, 16)


def make_inputs():
    rnd = random.Random(20241029)
    text = (b"The quick brown fox jumps over the lazy dog. " * 2000)[:65536]
    csv = CSV.read_bytes()
    mixed = b"".join(
        [
            csv[:262144],
            rnd.randbytes(262144),
            (b"abcabcabd" * 30000)[:262144],
            bytes(rnd.choice(b"\x00\x00\x00\x01\xff") for _ in range(262144 - 4096)),
            csv[-4096:],
        ]
    )
    return {
        "empty": b"",
        "one_byte": b"A",
        "text": text,
        "random64k": rnd.randbytes(65536),
        "mixed1m": mixed,
    }


ENCODE_CHUNK = 16384


def encode(data, order, mem, endmark):
    """Encode in small chunks.

    A single Ppmd7Encoder.encode() call on a large input can silently drop a byte when an
    internal output block fills up in the middle of a symbol (e.g. mixed1m, order 32, 1 MiB),
    producing a stream no decoder can read.  Chunked encoding avoids that encoder bug.
    """
    enc = pyppmd.Ppmd7Encoder(order, mem)
    out = b"".join(enc.encode(data[i : i + ENCODE_CHUNK]) for i in range(0, len(data), ENCODE_CHUNK))
    out += enc.flush(endmark=endmark)
    return out


def threaded_decode(stream, length, order, mem):
    """Decode with the legacy threaded decoder, mimicking py7zr usage.

    The threaded decoder stops as soon as its input buffer is empty, so py7zr feeds b"\\0"
    until the expected length is produced.  Returns (output, number_of_padding_bytes_fed).
    """
    dec = pyppmd.Ppmd7Decoder(order, mem)
    result = b""
    pos = 0
    padding = 0
    while len(result) < length:
        chunk = stream[pos : pos + 16384]
        pos += len(chunk)
        if not chunk:
            chunk = b"\0"  # py7zr workaround at end of input
            padding += 1
            assert padding <= 1024, "threaded decoder did not converge"
        result += dec.decode(chunk, length - len(result))
    return result, padding


def legacy_check(stream_file, input_file, order, mem):
    """Run threaded_decode in a subprocess; the legacy decoder may crash on some inputs.

    Returns ("ok", padding_bytes) or ("crash"|"mismatch"|"error", detail).
    """
    code = (
        "import sys, pathlib; sys.path.insert(0, %r); import generate as g; "
        "s = pathlib.Path(%r).read_bytes(); d = pathlib.Path(%r).read_bytes(); "
        "r, p = g.threaded_decode(s, len(d), %d, %d); "
        "print('ok' if r == d else 'mismatch', p)" % (str(HERE), str(stream_file), str(input_file), order, mem)
    )
    proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, timeout=300)
    if proc.returncode < 0:
        return "crash", -proc.returncode
    if proc.returncode != 0:
        return "error", proc.stderr.strip().splitlines()[-1] if proc.stderr.strip() else proc.returncode
    status, padding = proc.stdout.split()
    return status, int(padding)


def main():
    (HERE / "inputs").mkdir(exist_ok=True)
    (HERE / "streams").mkdir(exist_ok=True)
    manifest = {"inputs": {}, "streams": []}
    for name, data in make_inputs().items():
        (HERE / "inputs" / f"{name}.bin").write_bytes(data)
        manifest["inputs"][name] = {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        for order in ORDERS:
            for mib in MEM_SIZES_MIB:
                for endmark in (False, True):
                    if endmark and name != "text":
                        continue
                    mem = mib << 20
                    stream = encode(data, order, mem, endmark)
                    suffix = ".endmark" if endmark else ""
                    fname = f"{name}.o{order}.m{mib}{suffix}.ppmd"
                    (HERE / "streams" / fname).write_bytes(stream)
                    legacy, padding = ("skipped", None)
                    if len(data) > 0:
                        legacy, padding = legacy_check(HERE / "streams" / fname, HERE / "inputs" / f"{name}.bin", order, mem)
                        print(fname, legacy, padding, flush=True)
                    manifest["streams"].append(
                        {
                            "file": fname,
                            "input": name,
                            "order": order,
                            "mem_size": mem,
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

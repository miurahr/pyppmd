#!/usr/bin/env bash
#
# Build the C extension in-place with AddressSanitizer and UndefinedBehaviorSanitizer,
# then run the Ppmd7 decoder tests against it.
#
# Usage: utils/asan_test.sh [extra pytest args...]
#   e.g. utils/asan_test.sh --hypothesis-profile=fuzz
#
# Environment:
#   PYTHON      python interpreter with the test requirements installed (default: python3)
#   KEEP_BUILD  set to 1 to keep the sanitized extension in src/ after the run
#   ASAN_TESTS  test files/node ids to run (default: the Ppmd7 decoder tests below)
#
# test_ppmd8_fuzzer is deselected: the threaded Ppmd8 code hangs intermittently (also on
# main, without sanitizers), and this script is about the Ppmd7 decoder.
#
# Requires gcc. Any ASan or UBSan report aborts the run with a non-zero exit status.

set -euo pipefail

PYTHON=${PYTHON:-python3}
ASAN_TESTS=${ASAN_TESTS:-tests/test_ppmd7.py tests/test_fuzzer.py tests/test_ppmd7_malformed.py}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

remove_build() {
    rm -f src/pyppmd/c/_ppmd*.so
}

# A stale (non-sanitized) build in src/ would hide the sanitized one.
remove_build
if [ "${KEEP_BUILD:-0}" != "1" ]; then
    # The sanitized extension cannot be loaded without LD_PRELOAD; do not leave it behind.
    trap remove_build EXIT
fi

export CC=${CC:-gcc}
export CFLAGS="-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined"
export LDFLAGS="-fsanitize=address,undefined"
"$PYTHON" setup.py -q build_ext --inplace --build-temp build/asan --force

export LD_PRELOAD="$("$CC" -print-file-name=libasan.so) $("$CC" -print-file-name=libubsan.so)"
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=1
export UBSAN_OPTIONS=print_stacktrace=1
export PYTHONMALLOC=malloc
export PYTHONPATH="$ROOT/src"

# Make sure the tests import the sanitized build from src/, not an installed pyppmd.
"$PYTHON" -c 'import pyppmd.c._ppmd as m; print("testing", m.__file__)'
"$PYTHON" -m pytest -p no:cacheprovider --timeout=120 \
    --deselect tests/test_fuzzer.py::test_ppmd8_fuzzer $ASAN_TESTS "$@"

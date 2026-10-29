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
#
# Requires gcc. Any ASan or UBSan report aborts the run with a non-zero exit status.

set -euo pipefail

PYTHON=${PYTHON:-python3}
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

LD_PRELOAD="$("$CC" -print-file-name=libasan.so) $("$CC" -print-file-name=libubsan.so)" \
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1 \
PYTHONMALLOC=malloc \
PYTHONPATH="$ROOT/src" \
    "$PYTHON" -m pytest -p no:cacheprovider --timeout=120 \
    tests/test_ppmd7.py tests/test_fuzzer.py tests/test_ppmd7_malformed.py "$@"

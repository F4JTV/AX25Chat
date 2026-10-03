#!/usr/bin/env bash
#
# End-to-end test of the application over the real modem core, headless.
# Generates test audio with Direwolf's gen_packets when available (or uses
# the WAV given as $2), streams it to tst_app on stdin followed by real-time
# silence so the carrier detect drops and the queued message goes out.
#
#   scripts/run_app_test.sh <build-dir> [test.wav]
#
# This file is part of AX25Chat.
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

BUILD="${1:-build}"
WAV="${2:-}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -z "$WAV" ]; then
    GEN="$(command -v gen_packets || true)"
    if [ -z "$GEN" ]; then
        echo "gen_packets (from a Dire Wolf build) is not on PATH; pass a WAV as the second argument" >&2
        exit 2
    fi
    cat > "$TMP/msgs.txt" <<'MSGS'
F1ABC-7>CHAT,WIDE1-1:Hello from the test generator
F1ABC-7>APZ25C:=4350.00N/00710.00E-AX25Chat embedded core test
F1XYZ>CHAT:second station answering
MSGS
    "$GEN" -o "$TMP/test.wav" -r 44100 "$TMP/msgs.txt" >/dev/null 2>&1
    WAV="$TMP/test.wav"
fi

export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
(
    cat "$WAV"
    python3 - <<'PY' 2>/dev/null || true
import sys, time
chunk = b"\x00" * 8820          # 0.1 s of 44.1 kHz 16-bit mono silence
for _ in range(300):
    sys.stdout.buffer.write(chunk)
    sys.stdout.buffer.flush()
    time.sleep(0.1)
PY
) | "$BUILD/tst_app"

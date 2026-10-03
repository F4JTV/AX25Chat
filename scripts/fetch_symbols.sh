#!/usr/bin/env bash
#
# Download the aprs.fi symbol sheets (https://github.com/hessu/aprs-symbols)
# into assets/aprs-symbols, with the COPYRIGHT.md that travels with them.
# The artwork is deliberately not committed to this repository because its
# copyright status is mixed; read COPYRIGHT.md before redistributing it.
#
#   scripts/fetch_symbols.sh [size]      size 64 (default) or 128
#
# This file is part of AX25Chat.
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIZE="${1:-64}"
DEST="$ROOT/assets/aprs-symbols"
BASE="https://raw.githubusercontent.com/hessu/aprs-symbols/master"

mkdir -p "$DEST"
for t in 0 1 2; do
    name="aprs-symbols-$SIZE-$t.png"
    echo "Fetching $name"
    curl -fsSL -o "$DEST/$name" "$BASE/png/$name"
done
curl -fsSL -o "$DEST/COPYRIGHT.md" "$BASE/COPYRIGHT.md"
echo "Symbol sheets ($SIZE px) in $DEST"

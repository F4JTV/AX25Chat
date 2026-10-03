#!/usr/bin/env bash
# ============================================================================
#  AX25Chat - build a Debian package
#
#  Compiles and packages the program on the machine this runs on. A .deb
#  carries compiled code, so one architecture equals one package; there is
#  no universal build. Written for Ubuntu 24.04 (Qt 6.4); Debian 13 and
#  derivatives work the same way.
#
#  Dependencies are not written by hand: dpkg-shlibdeps reads the libraries
#  actually linked into the program and names the packages that provide
#  them.
#
#  Usage:
#    ./make_deb.sh             build the package
#    ./make_deb.sh --symbols   include the aprs.fi symbol artwork (fetched if
#                              missing; read its COPYRIGHT.md before you
#                              redistribute the package)
#    ./make_deb.sh --check     run lintian on the result
#    ./make_deb.sh --deps      install the build dependencies first (sudo)
#    ./make_deb.sh --no-deps   never check them, and do not ask
#    ./make_deb.sh --clean     start from an empty build directory
#    ./make_deb.sh --help
# ============================================================================
set -uo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SRC_DIR/build-deb"
DO_SYMBOLS=0
DO_CHECK=0
DO_CLEAN=0
DO_DEPS=ask          # ask | yes | no

# shellcheck source=packaging/build-deps.sh
. "$SRC_DIR/packaging/build-deps.sh"

while [ $# -gt 0 ]; do
    case "$1" in
        --symbols) DO_SYMBOLS=1 ;;
        --check)   DO_CHECK=1 ;;
        --clean)   DO_CLEAN=1 ;;
        --deps)    DO_DEPS=yes ;;
        --no-deps) DO_DEPS=no ;;
        -h|--help)
            sed -n '2,24p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) die "Unknown option: $1  (try --help)" ;;
    esac
    shift
done

step "Checking prerequisites"
[ -f "$SRC_DIR/CMakeLists.txt" ] || die "Run this from the project directory."

AX_DEPS_MODE="$DO_DEPS"
AX_EXTRA_DEPS="dpkg-dev"     # dpkg-shlibdeps, to work out the dependencies
[ "$DO_CHECK" -eq 1 ] && AX_EXTRA_DEPS="$AX_EXTRA_DEPS lintian"
ax_install_deps || true

command -v cmake >/dev/null 2>&1 || die "cmake missing: sudo apt install cmake"
command -v ninja >/dev/null 2>&1 || die "ninja missing: sudo apt install ninja-build"
command -v cpack >/dev/null 2>&1 || die "cpack missing: it ships with cmake"
command -v dpkg-shlibdeps >/dev/null 2>&1 || \
    die "dpkg-shlibdeps missing: sudo apt install dpkg-dev"
command -v gzip >/dev/null 2>&1 || die "gzip missing: sudo apt install gzip"

ax_ensure_direwolf "$SRC_DIR"

VERSION="$(sed -n 's/^project(AX25Chat VERSION \([0-9.]*\).*/\1/p' "$SRC_DIR/CMakeLists.txt" | head -1)"
ARCH="$(dpkg --print-architecture)"
say "Version       ${VERSION:-unknown}"
say "Architecture  $ARCH"
if [ -r /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    say "System        ${PRETTY_NAME:-unknown}"
fi

if [ "$DO_SYMBOLS" -eq 1 ]; then
    if [ ! -f "$SRC_DIR/assets/aprs-symbols/aprs-symbols-64-0.png" ]; then
        step "Symbol artwork"
        "$SRC_DIR/scripts/fetch_symbols.sh" || die "Could not fetch the symbol artwork."
    fi
    say "Symbol artwork: included (see assets/aprs-symbols/COPYRIGHT.md)"
else
    say "Symbol artwork: not included (--symbols to add it)"
fi

# The maintainer scripts must be executable inside the package; the bit
# gets lost when the sources are copied through some file systems.
chmod 755 "$SRC_DIR/packaging/deb/postinst" "$SRC_DIR/packaging/deb/postrm" 2>/dev/null || true

if [ "$DO_CLEAN" -eq 1 ] && [ -d "$BUILD_DIR" ]; then
    step "Cleaning"
    rm -rf "$BUILD_DIR"
    say "$BUILD_DIR removed"
fi
rm -f "$BUILD_DIR"/*.deb 2>/dev/null

step "Building"
SYMBOLS_FLAG=""
[ "$DO_SYMBOLS" -eq 0 ] && SYMBOLS_FLAG="-DAX25CHAT_PACKAGE_SYMBOLS=OFF"
# shellcheck disable=SC2086
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DAX25CHAT_BUILD_TESTS=OFF \
      -DAX25CHAT_BUILD_TOOLS=OFF \
      $SYMBOLS_FLAG >/dev/null || die "Configuration failed."
cmake --build "$BUILD_DIR" -j"$(ax_build_jobs)" || die "Compilation failed."

step "Packaging"
# cpack runs from the build directory: dpkg-shlibdeps looks for the program
# there.
( cd "$BUILD_DIR" && cpack -G DEB ) || die "Packaging failed."

DEB="$(ls -t "$BUILD_DIR"/*.deb 2>/dev/null | head -1)"
[ -n "$DEB" ] || die "No .deb produced."
cp -f "$DEB" "$SRC_DIR/" && DEB="$SRC_DIR/$(basename "$DEB")"

step "Result"
say "File          $DEB"
say "Size          $(du -h "$DEB" | cut -f1)"
say "Dependencies:"
dpkg-deb -f "$DEB" Depends | tr ',' '\n' | sed 's/^ */    /'

if [ "$DO_CHECK" -eq 1 ]; then
    step "lintian"
    if ! command -v lintian >/dev/null 2>&1; then
        say "lintian missing: sudo apt install lintian"
    else
        lintian --tag-display-limit 0 "$DEB" || true
    fi
fi

step "Install with"
say "sudo apt install $DEB"
say "(apt, not dpkg -i: it pulls the dependencies on its own)"

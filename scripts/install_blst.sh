#!/usr/bin/env bash
set -euo pipefail

BLST_VERSION="v0.3.11"
BLST_REPOSITORY_URL="https://github.com/supranational/blst.git"
BLST_ARCHIVE_URL="https://github.com/supranational/blst/archive/refs/tags/${BLST_VERSION}.tar.gz"
INSTALL_PREFIX="${BLST_INSTALL_PREFIX:-"$HOME/.nodo/deps/blst"}"

if [ -z "${HOME:-}" ]; then
    echo "Error: HOME is not set. Cannot choose an external blst install directory."
    exit 1
fi

if ! command -v cc >/dev/null 2>&1 && ! command -v gcc >/dev/null 2>&1 && ! command -v clang >/dev/null 2>&1; then
    echo "Error: no C compiler was found in PATH."
    echo "Install a C compiler first:"
    echo "  Linux: use your distribution build tools package"
    echo "  macOS: xcode-select --install"
    echo "  MSYS2 UCRT64: pacman -S mingw-w64-ucrt-x86_64-gcc"
    exit 1
fi

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/nodo-blst.XXXXXX")"
SOURCE_DIR="$WORK_DIR/blst"

cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

download_blst() {
    if command -v git >/dev/null 2>&1; then
        git clone --depth 1 --branch "$BLST_VERSION" "$BLST_REPOSITORY_URL" "$SOURCE_DIR"
        return
    fi

    if command -v curl >/dev/null 2>&1 && command -v tar >/dev/null 2>&1; then
        mkdir -p "$SOURCE_DIR"
        curl -L "$BLST_ARCHIVE_URL" | tar -xz --strip-components=1 -C "$SOURCE_DIR"
        return
    fi

    echo "Error: install_blst.sh needs either git, or curl plus tar, to download blst."
    exit 1
}

download_blst

cd "$SOURCE_DIR"
chmod +x ./build.sh
./build.sh

HEADER_PATH="$SOURCE_DIR/bindings/blst.h"
if [ ! -f "$HEADER_PATH" ]; then
    echo "Error: blst build did not produce bindings/blst.h."
    exit 1
fi

LIBRARY_PATH="$(
    find "$SOURCE_DIR" -maxdepth 3 -type f \
        \( -name 'libblst.a' -o -name 'libblst.dll.a' -o -name 'libblst.dylib' -o -name 'blst.lib' \) \
        | sort \
        | head -n 1
)"

if [ -z "$LIBRARY_PATH" ] || [ ! -f "$LIBRARY_PATH" ]; then
    echo "Error: blst build did not produce a library file."
    exit 1
fi

mkdir -p "$INSTALL_PREFIX/include" "$INSTALL_PREFIX/lib"
cp "$SOURCE_DIR"/bindings/*.h "$INSTALL_PREFIX/include/"
cp "$LIBRARY_PATH" "$INSTALL_PREFIX/lib/"

echo
echo "blst installed outside the Nodo repository:"
echo "  $INSTALL_PREFIX"
echo
echo "CMake will find it automatically, or you can pass:"
echo "  -DBLST_ROOT=$INSTALL_PREFIX"

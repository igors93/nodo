#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
    echo "Build directory removed."
else
    echo "Build directory does not exist. Nothing to clean."
fi
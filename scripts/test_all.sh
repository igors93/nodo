#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Nodo unified CMake/CTest runner"
echo "------------------------------"

"$ROOT_DIR/scripts/cmake_test_all.sh"

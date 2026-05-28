#!/usr/bin/env bash
set -euo pipefail

# Nodo unified test runner.
# This script runs all available project-level tests in a deterministic order.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Nodo unified test runner"
echo "------------------------"
echo

required_scripts=(
    "$ROOT_DIR/scripts/test_serialization.sh"
    "$ROOT_DIR/scripts/test_storage.sh"
)

for script_path in "${required_scripts[@]}"; do
    if [[ ! -f "$script_path" ]]; then
        echo "Error: required test script was not found: $script_path"
        exit 1
    fi

    if [[ ! -x "$script_path" ]]; then
        echo "Error: required test script is not executable: $script_path"
        echo "Run: chmod +x scripts/*.sh"
        exit 1
    fi
done

echo "Running serialization tests..."
"$ROOT_DIR/scripts/test_serialization.sh"

echo
echo "Running blockchain storage integration tests..."
"$ROOT_DIR/scripts/test_storage.sh"

echo
echo "All Nodo tests completed successfully."

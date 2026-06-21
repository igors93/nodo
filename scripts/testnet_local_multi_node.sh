#!/usr/bin/env bash
set -euo pipefail

# Local multi-node testnet script.
#
# Initializes up to NODE_COUNT independent Nodo nodes in separate data directories,
# submits transactions, produces blocks, then runs reload and chain audit on each.
#
# Usage:
#   ./scripts/testnet_local_multi_node.sh           # fresh 4-node start
#   ./scripts/testnet_local_multi_node.sh --resume  # skip init if data dirs exist
#   ./scripts/testnet_local_multi_node.sh --clean   # remove testnet dirs and exit
#
# Environment overrides (export before running):
#   NODO_TESTNET_DIR   - root directory for node data (default: <repo>/testnet/local)
#   NODO_NODE_COUNT    - number of nodes to start (default: 4)
#   NODO_BLOCKS        - blocks to produce per node (default: 3)
#   NODO_BASE_PORT     - first TCP port for peer endpoints (default: 30330)
#   NODO_BUILD_JOBS    - parallel build jobs (default: 1)

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NODO="$ROOT_DIR/build/nodo"
TESTNET_DIR="${NODO_TESTNET_DIR:-$ROOT_DIR/testnet/local}"
NODE_COUNT="${NODO_NODE_COUNT:-4}"
BLOCKS_PER_NODE="${NODO_BLOCKS:-3}"
BASE_PORT="${NODO_BASE_PORT:-30330}"

RESUME=false
CLEAN=false

for arg in "$@"; do
    case "$arg" in
        --resume) RESUME=true ;;
        --clean)  CLEAN=true  ;;
    esac
done

# -- helpers ------------------------------------------------------------------

ts() { date '+%Y-%m-%dT%H:%M:%S'; }

log() {
    local id="$1"
    local msg="$2"
    echo "[$(ts)][$id] $msg"
    echo "[$(ts)][$id] $msg" >> "$TESTNET_DIR/$id.log" 2>/dev/null || true
}

node_dir()  { echo "$TESTNET_DIR/node-$1"; }
node_log()  { echo "$TESTNET_DIR/node-$1.log"; }
node_port() { echo $((BASE_PORT + $1)); }
node_id()   { echo "node-$1"; }

run_nodo() {
    local id="$1"; shift
    local log_file; log_file="$(node_log "${id#node-}")"
    "$NODO" "$@" >> "$log_file" 2>&1
}

# -- clean mode ---------------------------------------------------------------

if [ "$CLEAN" = true ]; then
    echo "[$(ts)][testnet] Removing $TESTNET_DIR"
    rm -rf "$TESTNET_DIR"
    echo "[$(ts)][testnet] Done."
    exit 0
fi

# -- build if needed ----------------------------------------------------------

if [ ! -x "$NODO" ]; then
    echo "[$(ts)][testnet] nodo binary not found — building..."
    NODO_BUILD_JOBS="${NODO_BUILD_JOBS:-1}" "$ROOT_DIR/scripts/cmake_build.sh"
fi

mkdir -p "$TESTNET_DIR"

echo ""
echo "=== Nodo Local Testnet ==="
printf "  Nodes:     %s\n" "$NODE_COUNT"
printf "  Blocks:    %s per node\n" "$BLOCKS_PER_NODE"
printf "  Directory: %s\n" "$TESTNET_DIR"
printf "  Binary:    %s\n" "$NODO"
echo ""

# -- Phase 1: Initialize nodes ------------------------------------------------

echo "[$(ts)][testnet] Phase 1: initialization"

for i in $(seq 0 $((NODE_COUNT - 1))); do
    ID="$(node_id $i)"
    DIR="$(node_dir $i)"
    PORT="$(node_port $i)"
    LOG="$(node_log $i)"

    # Truncate log for fresh runs; preserve for --resume.
    if [ "$RESUME" = false ]; then
        : > "$LOG"
    fi

    if [ "$RESUME" = true ] && [ -f "$DIR/manifest.nodo" ]; then
        log "$ID" "Resuming — data directory exists, skipping init."
        continue
    fi

    log "$ID" "Initializing at $DIR (endpoint 127.0.0.1:$PORT)..."
    rm -rf "$DIR"
    run_nodo "$ID" init \
        --data-dir "$DIR" \
        --peer-id  "$ID" \
        --endpoint "127.0.0.1:$PORT"

    log "$ID" "Creating keys..."
    run_nodo "$ID" keys create --data-dir "$DIR"

    log "$ID" "Init complete."
done

echo ""

# -- Phase 2: Produce blocks --------------------------------------------------

echo "[$(ts)][testnet] Phase 2: block production"

for i in $(seq 0 $((NODE_COUNT - 1))); do
    ID="$(node_id $i)"
    DIR="$(node_dir $i)"

    log "$ID" "Producing $BLOCKS_PER_NODE block(s)..."

    for b in $(seq 1 "$BLOCKS_PER_NODE"); do
        log "$ID" "  tx submit (block $b/$BLOCKS_PER_NODE)..."
        run_nodo "$ID" tx submit --data-dir "$DIR"

        log "$ID" "  block produce $b/$BLOCKS_PER_NODE..."
        run_nodo "$ID" block produce --data-dir "$DIR"
    done

    log "$ID" "Block production done."
done

echo ""

# -- Phase 3: Reload and audit ------------------------------------------------

echo "[$(ts)][testnet] Phase 3: reload + audit"

PASS=0
FAIL=0

for i in $(seq 0 $((NODE_COUNT - 1))); do
    ID="$(node_id $i)"
    DIR="$(node_dir $i)"
    PORT="$(node_port $i)"

    log "$ID" "Reloading state..."
    if run_nodo "$ID" node reload \
            --data-dir "$DIR" \
            --peer-id  "$ID" \
            --endpoint "127.0.0.1:$PORT"; then
        log "$ID" "Reload OK."
    else
        log "$ID" "ERROR: reload failed — see $(node_log $i)"
        FAIL=$((FAIL + 1))
        continue
    fi

    log "$ID" "Running chain audit..."
    if run_nodo "$ID" chain audit \
            --data-dir "$DIR" \
            --peer-id  "$ID" \
            --endpoint "127.0.0.1:$PORT"; then
        log "$ID" "Audit PASSED."
        PASS=$((PASS + 1))
    else
        log "$ID" "ERROR: audit FAILED — see $(node_log $i)"
        FAIL=$((FAIL + 1))
    fi
done

echo ""

# -- Summary ------------------------------------------------------------------

echo "=== Testnet Summary ==="

for i in $(seq 0 $((NODE_COUNT - 1))); do
    ID="$(node_id $i)"
    DIR="$(node_dir $i)"
    echo ""
    echo "  [$ID]"
    "$NODO" status --data-dir "$DIR" 2>/dev/null | sed 's/^/    /' \
        || echo "    (status unavailable)"
done

echo ""

if [ "$FAIL" -eq 0 ]; then
    echo "RESULT: All $PASS/$NODE_COUNT node(s) passed reload + audit."
    echo "Logs:   $TESTNET_DIR/node-*.log"
    exit 0
else
    echo "RESULT: $PASS passed, $FAIL FAILED."
    echo "Logs:   $TESTNET_DIR/node-*.log"
    exit 1
fi

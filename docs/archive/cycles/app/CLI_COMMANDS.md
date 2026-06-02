> Archived document.
> This file is kept for historical context only and may not describe the current Nodo implementation.

# Nodo CLI Commands

Status: Cycle 7 Implementation  
Version: NODO-CLI-V1

## Purpose

This phase adds the first operator-facing CLI commands.

## Commands

```bash
nodo help
nodo demo
nodo init [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo status [--data-dir PATH]
nodo inspect [--data-dir PATH]
```

## Init

`nodo init` creates the local node data directory and writes:

```text
manifest.nodo
genesis.nodo
peers/local_peer.nodo
runtime/runtime_snapshot.nodo
```

## Status

`nodo status` prints a short operational summary.

## Inspect

`nodo inspect` prints the serialized manifest for deeper debugging.

## Safety

The CLI refuses to initialize an existing data directory with a different
genesis id. This prevents accidental network identity corruption.

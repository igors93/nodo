# Protocol Overview

Nodo's protocol is composed of five main layers:

1. network admission and message exchange;
2. transaction admission and mempool policy;
3. block proposal and state transition;
4. consensus voting and finalization evidence;
5. persistence, reload, sync, and audit.

## Protocol objectives

- deterministic state transition;
- canonical serialization;
- weighted finality evidence;
- evidence-backed governance, treasury, reward, and penalty records;
- state roots that include account and protocol-domain commitments;
- safe reload from persisted artifacts;
- rejection of non-canonical or unverifiable input.

## Finalized artifact model

A finalized artifact should contain enough information for another node to verify:

- block identity and height;
- previous block relationship;
- transaction list and receipts;
- pre/post state commitments;
- validator and quorum evidence;
- ledger records;
- governance records;
- treasury records;
- reward and penalty records;
- slashing evidence;
- serialization and storage compatibility.

## Safety posture

Nodo should prefer rejection over ambiguous acceptance. If a node cannot prove that a block, transaction, file, or protocol-domain record is valid, it should not silently continue.

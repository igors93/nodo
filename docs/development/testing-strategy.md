# Testing Strategy

Nodo's tests are organized by module and discovered automatically by CMake.

## What Tests Should Prove

- protocol records validate required fields;
- canonical codecs reject missing and unexpected fields;
- storage reload rejects corrupted data;
- consensus rejects duplicate, stale, or conflicting votes;
- governance lifecycle verification rebuilds votes, tally, and decision;
- treasury evidence rejects direct approvals without lifecycle context;
- monetary reports and supply audit match finalized history;
- P2P validators reject wrong network or malformed messages.

## Running Tests

See [Testing](../getting-started/testing.md).

## Regression Tests

Every security fix should add a regression test that fails against the old behavior and passes after the fix. Keep regression tests small enough to show the invariant directly.

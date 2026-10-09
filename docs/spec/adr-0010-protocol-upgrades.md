# ADR 0010: Height-activated, content-addressed protocol upgrades

**Status:** accepted Phase 1 design decision. This is the v1 consensus
contract; the `nodo/0.7` development runtime does not implement it. Phase 7.1
remains a mandatory implementation and release gate.

## Decision

Genesis commits to protocol version 1 and its nonzero `rule_set_hash`. Each
non-genesis header commits to `protocol_version`, `rule_set_hash`,
`next_protocol_version` and `next_rule_set_hash`. All four are checked against
the finalized upgrade schedule, never accepted as a peer's choice. A version
is a `u16`; version 0, version skips, hash substitutions and reuse of an
activated version number for different rules are invalid. A canceled pending
version was never activated and may be proposed again only through a fresh
governance action. The eight-byte top-level object prefix
has the same version as the active header. A future version publishes its own
complete, unambiguous schemas and hash/signature rules before it can be voted
in. `nodo/0.x` build strings are not protocol versions.

The immutable rule-set commitment is
`H("RULESET", u16(version) || rule_bundle_hash || vectors_hash ||
migration_hash)`, with 32-byte digests in that order. `rule_bundle_hash` and
`vectors_hash` MUST be nonzero; all-zero `migration_hash` means no migration.
The bundle is a versioned release artifact whose exact byte manifest defines
every consensus rule, limit, codec, cryptographic domain and deterministic
state transition. The conformance artifact contains executable, positive and
negative cross-implementation vectors. Both artifacts MUST be published and
independently reproducible before the governance vote. Each manifest is
`u32(file_count)` followed by strictly ascending entries
`path:str, content:bytes`; paths are nonempty ASCII relative paths of at most
256 bytes, using `/` between segments and `[a-z0-9._-]` inside each segment,
with no empty, `.` or `..` segment and no duplicates. Contents are raw bytes
with no newline or archive metadata conversion. File count MUST be between
1 and 4096; manifest bytes are capped at 16777216. Empty file contents are
allowed. The hashes are
`rule_bundle_hash = H("RULE-BUNDLE", bundle_manifest_bytes)` and
`vectors_hash = H("RULE-VECTORS", vectors_manifest_bytes)`. A nonzero
migration hash is `H("RULE-MIGRATION", migration_manifest_bytes)` under the
same canonical format; that artifact fixes the deterministic input-to-output
state-root calculation, resource bounds and failure conditions. A digest is a
commitment, not permission to download and execute untrusted code. A validator
MUST install,
verify and test the implementation and artifacts out of band before signing.
Genesis and every approved upgrade MUST identify artifacts whose bytes and
hashes match the committed digests. The genesis v1 rule hash is pinned out of
band together with its genesis hash.

The v1 governance action `PROTOCOL_UPGRADE` has the exact binary payload
`next_version:u16, activation_height:u64, rule_bundle_hash:hash,
vectors_hash:hash, migration_hash:hash`. Execution derives the rule-set hash
and stores one schedule entry in authenticated state. It uses the ordinary
snapshot-weight vote, strict quorum, approval threshold, timelock, expiry,
deposit and once-only execution of protocol v1 section 3. The action is
invalid unless `next_version = active_version + 1`, no other noncanceled
upgrade is pending, the activation height has never been used by any schedule
entry (including a canceled entry), the hashes are valid and the height is the
first block of a future epoch. Only one pending upgrade is permitted, so the
next version cannot be proposed against a speculative version. Governance
approval does not itself change the active rules.

If the approved action executes at positive height `h` in epoch
`e = floor((h-1)/L)`, its earliest activation is the first height of epoch
`e+5`, namely `(e+5)*L+1`. Epochs `e+1` through `e+4` must therefore complete
under the old rules. A later epoch boundary may be named, but the arithmetic
must fit `u64`. Epoch length `L` is the immutable genesis value. Neither BFT
time, elapsed wall time, software readiness nor peer votes can advance or
delay activation.

A distinct `UPGRADE_CANCEL` governance action has exact payload
`activation_height:u64, rule_set_hash:hash` and uses the same voting and
execution requirements. It must identify the sole pending entry. Its execution
must finalize at `h < activation_height` and precede the old-rule boundary
block's commitments. It changes the entry to canceled, leaving an auditable
historical record. After cancellation, a replacement requires a fresh
proposal, approval, timelock and four full future epochs. No cancellation or
rollback is possible after the activation height. A contradictory approval
or cancellation makes the containing block invalid.

At every height `h`, the active fields equal the rules selected by genesis
and the authenticated, finalized schedule at `h`. The `next_*` fields equal
the rules selected for `h+1`. Usually both pairs are equal. If an uncanceled
upgrade activates at `A`, the finalized block at `A-1`, still decoded and
executed entirely under the old rules, MUST commit exactly the approved
next version and hash. Its parent/finality QC and state root authenticate
that commitment. The first block at `A` MUST use the committed next rules,
including the new top-level prefix and codec; a block with the old version,
an unknown hash or an unscheduled transition is invalid. A validator that
cannot execute the next rules MUST stop validation/signing at `A`, retain its
state and report the missing version. It MUST NOT silently continue on old
rules, guess new rules from a header, reinterpret historical bytes, or apply
an operator override. If the required implementation is unavailable, the
safe outcome is a halt. Operators should cancel through governance before
`A-1` if the release cannot be safely activated.

Activation begins from the exact old-rule finalized state at `A-1`. The first
new-version block MUST verify its parent QC with the old version's codec,
signature domains, validator set and quorum, even if the new version changes
any of them. If the new rule set includes a migration, its pinned deterministic
migration runs once before user transactions at `A`, with checked input and output roots,
bounded resources and fully specified failure behavior. No migration may
mutate an earlier finalized state or bypass supply, evidence, unbonding or
other historical obligations. The active validator and parameter sets at
`A` are those committed by the old-rule boundary at `A-1`; a migration may
schedule changes only for later legal boundaries. In-flight transactions are
revalidated under the new rules; mempools evict those that fail. Historical
evidence is decoded under the signed object's historical version and remains
slashable for the original evidence window. Every earlier codec, rule bundle,
schedule event and migration commitment required by the accepted chain must
remain available for replay and audit.

Snapshots and checkpoints are accepted only after verifying their header/QC
path and matching their declared protocol version and rule hash to that
history. Network STATUS advertises the peer's finalized version/hash, its
next scheduled activation and supported versions for coordination, but is
advisory. A readiness bit, peer majority or local software installation is
never an activation vote. Nodes SHOULD avoid requesting objects they cannot
decode and MUST reject unknown versions without weakening validation.

## Required implementation and release gate

Phase 2.1 must implement exact codecs for the genesis, header, snapshot,
governance actions, state leaves, system records and STATUS fields fixed by
this ADR. Phase 3.15 must make migrations atomic and crash recoverable. Phase
7.1 must enforce proposal authorization, scheduling, cancellation, the
boundary QC, activation, historical dispatch, fail-closed signing and
independent replay in every path (producer, validator, sync, snapshot import
and light client). Release engineering must publish reproducible artifacts,
cross-version fixtures and a staged testnet rehearsal. The v1 chain MUST NOT
activate until these gates pass independent review. An incompatible recovery
after activation requires a separately specified new network identity and
explicit social coordination; it is not an unrecorded rollback.

The checked `V1UpgradeSchedule` C++ implementation is a Phase 1 arithmetic
and commitment reference. It does not authorize governance actions, persist
state or execute future versions.

# Governance Vote Evidence

Governance vote evidence binds a vote to the context that makes it meaningful.

## Required Context

Each evidence record includes:

- `evidenceId`;
- `GovernanceProposalEnvelope`;
- `GovernanceVotingPolicy`;
- `GovernanceVoteRecord`.

The vote record includes:

- vote id;
- governance proposal id;
- voter id;
- vote choice;
- voting power;
- cast block;
- voting power source;
- deterministic vote proof;
- policy version.

## Verification

Vote evidence is rejected when:

- the evidence id is empty;
- the proposal envelope is invalid;
- the voting policy is invalid;
- the vote record is invalid;
- the vote targets a different proposal;
- the vote uses a different policy version;
- the vote was cast before proposal submission;
- the vote is below minimum voting power;
- abstain is disallowed by policy;
- the deterministic vote proof does not match the record.

## Vote Set Audit

The vote-set audit rejects duplicate evidence ids, duplicate vote ids, duplicate voters, wrong proposals, policy mismatches, and unimplemented vote replacement. Accepted votes are returned in deterministic canonical order for tally rebuilding.

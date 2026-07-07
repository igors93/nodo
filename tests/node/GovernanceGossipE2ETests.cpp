// Real multi-node TCP end-to-end test (roadmap item 4.1): governance
// proposals and votes travel as ordinary transactions over TRANSACTION_GOSSIP
// — no dedicated network message type — and are admitted and executed
// identically on every node.
//
// Scenario:
//   1. A governance proposal is written directly to node A's persistent
//      mempool directory, exactly as the "nodo governance propose" CLI
//      command does, while node A's daemon is already running. This proves
//      the daemon picks up and gossips locally-submitted transactions instead
//      of only reloading them from disk at the next process restart.
//   2. The proposal becomes visible (votable) on nodes B and C purely through
//      gossip + consensus, without ever being submitted to them directly.
//   3. Each of the 3 validators' owners casts a vote through a different
//      node's RPC endpoint (including node B, voting on a proposal that
//      originated at node A) — TransactionAdmissionPolicy admits governance
//      votes exactly like any other transaction type.
//   4. Once every vote is finalized, the tally reported by all 3 nodes is
//      byte-for-byte identical, and the proposal reaches EXECUTED
//      identically everywhere.
//   5. A second vote from the same validator is rejected (defense in depth:
//      the same duplicate-vote check that runs at admission also runs at
//      execution).

#include "../common/RealTcpNodeTestSupport.hpp"

#include "node/NodeDataDirectory.hpp"
#include "node/PersistentMempoolStore.hpp"

namespace {

#ifndef _WIN32

bool accountHasBalance(const NodeSpec &spec, const std::string &address) {
  const auto response = httpRequest(spec.rpcPort, "GET", "/account/" + address);
  return response.has_value() &&
         jsonUnsigned(response->body, "balance").value_or(0) > 0;
}

// This chain never produces empty blocks — a proposer with nothing pending
// in its mempool simply does not propose (see MempoolBlockProducer). Letting
// a governance proposal's voting period elapse therefore requires actively
// keeping transactions flowing, one at a time so each is very likely to land
// in its own block, rather than just waiting on a clock. Submits filler
// transfers (continuing nextNonce) until predicate() is true or the block
// budget runs out.
void driveChainUntil(const NodeSpec &driverNode,
                     const config::GenesisConfig &genesis,
                     const std::string &seedPrefix, std::uint64_t &nextNonce,
                     int maxFillerBlocks,
                     const std::function<bool()> &predicate) {
  for (int attempt = 0; attempt < maxFillerBlocks; ++attempt) {
    if (predicate()) {
      return;
    }

    // Only advance nextNonce once the submission is actually accepted. A
    // transient rejection (e.g. the RPC per-source-IP rate limit, since each
    // iteration also polls) must retry the same nonce — advancing regardless
    // would leave a nonce gap that permanently stalls block production
    // (transactionsForBlock requires contiguous nonces per sender), and 28
    // stuck filler transactions is exactly the failure this used to produce.
    const core::Transaction filler =
        signedTransfer(genesis, seedPrefix, "govgossip-filler-recipient",
                       nextNonce, unixTime());
    const auto submitted = submitTransaction(driverNode, filler);
    const bool accepted =
        submitted.has_value() &&
        submitted->body.find("\"status\":\"ACCEPTED\"") != std::string::npos;
    const std::uint64_t fillerNonce = nextNonce;
    if (accepted) {
      ++nextNonce;
    }

    waitUntil(5s, [&] {
      const auto response =
          httpRequest(driverNode.rpcPort, "GET",
                      "/account/" + testUserKey(seedPrefix).address().value());
      if (response.has_value()) {
        const std::uint64_t currentNonce = jsonUnsigned(response->body, "nonce").value_or(0);
        if (currentNonce >= fillerNonce) {
          nextNonce = std::max(nextNonce, currentNonce + 1);
          return true;
        }
      }
      return false;
    });
  }
}

void testGovernanceProposalAndVoteGossip() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nodo-governance-gossip-e2e-" + std::to_string(::getpid()));
  std::error_code cleanupError;
  std::filesystem::remove_all(root, cleanupError);

  const std::string seedPrefix = "govgossip";
  const NodeSpecs specs = makeNodeSpecs(root, seedPrefix);
  const config::GenesisConfig genesis =
      makeGenesis(specs, unixTime() - 5, seedPrefix);
  ChildProcesses nodes(specs, genesis, fullMeshTopology());

  const NodeSpec &nodeA = specs[0];
  const NodeSpec &nodeB = specs[1];
  const NodeSpec &nodeC = specs[2];

  try {
    nodes.startAll();
    waitForRpc(nodeA);
    waitForRpc(nodeB);
    waitForRpc(nodeC);

    require(waitUntil(120s,
                      [&] {
                        return authenticatedPeerCount(nodeA) == 2 &&
                               authenticatedPeerCount(nodeB) == 2 &&
                               authenticatedPeerCount(nodeC) == 2;
                      }),
            "Nodes did not form a full mesh.");

    // Fund each validator's owner account: governance votes are mempool
    // transactions signed by the owner, and a brand-new account cannot send
    // one until it exists in account state.
    const std::int64_t now = unixTime();
    const core::Transaction fundOwnerA = signedTransfer(
        genesis, seedPrefix, nodeA.ownerKey.address().value(), 1, now);
    const core::Transaction fundOwnerB = signedTransfer(
        genesis, seedPrefix, nodeB.ownerKey.address().value(), 2, now);
    const core::Transaction fundOwnerC = signedTransfer(
        genesis, seedPrefix, nodeC.ownerKey.address().value(), 3, now);

    for (const core::Transaction &funding :
         {fundOwnerA, fundOwnerB, fundOwnerC}) {
      const auto submitted = submitTransaction(nodeA, funding);
      require(submitted.has_value() && submitted->statusCode == 200 &&
                  submitted->body.find("\"status\":\"ACCEPTED\"") !=
                      std::string::npos,
              "Owner funding transfer was not accepted: " +
                  (submitted.has_value() ? submitted->body : "no response"));
    }

    // The 3 funding transfers were submitted back to back and likely all
    // land in the same block (this chain batches every pending mempool
    // transaction it can into one block, up to maxTransactionsPerBlock), so
    // wait on the resulting account balances rather than on a specific
    // finalized height.
    require(waitUntil(60s,
                      [&] {
                        return accountHasBalance(
                                   nodeA, nodeA.ownerKey.address().value()) &&
                               accountHasBalance(
                                   nodeB, nodeB.ownerKey.address().value()) &&
                               accountHasBalance(
                                   nodeC, nodeC.ownerKey.address().value());
                      }),
            "Owner funding transfers did not finalize on all 3 nodes.");

    // Step 1: submit the proposal the same way the CLI does — write it
    // straight to node A's persistent mempool directory, bypassing RPC
    // entirely, while node A's daemon is already running.
    const core::Transaction proposal = signedGovernanceProposal(
        genesis, seedPrefix, /*nonce=*/4, unixTime(),
        /*votingPeriodBlocks=*/10, /*quorumNumerator=*/1,
        /*quorumDenominator=*/2, /*approvalNumerator=*/1,
        /*approvalDenominator=*/2);
    const std::string proposalId = proposal.id();

    const node::PersistentMempoolWriteResult persisted =
        node::PersistentMempoolStore::persistTransaction(
            node::NodeDataDirectoryConfig(nodeA.dataDirectory), proposal,
            testUserKey(seedPrefix).publicKey(), unixTime());
    require(persisted.success(),
            "Failed to persist the governance proposal to node A's mempool "
            "directory: " +
                persisted.reason());

    // Step 2: the proposal must become votable on B and C purely through
    // gossip + consensus — neither ever received it directly.
    require(waitUntil(60s,
                      [&] {
                        return governanceProposalVisible(nodeA, proposalId) &&
                               governanceProposalVisible(nodeB, proposalId) &&
                               governanceProposalVisible(nodeC, proposalId);
                      }),
            "Governance proposal submitted at node A never became votable "
            "on nodes B and C.");

    // Step 3: each validator's owner votes through a different node's RPC,
    // including node B — a vote on a proposal that originated at node A.
    const core::Transaction voteA =
        signedGovernanceVote(genesis, proposalId, nodeA.ownerKey,
                             nodeA.validatorKey.address().value(),
                             core::GovernanceVoteChoice::YES, 1, unixTime());
    const core::Transaction voteB =
        signedGovernanceVote(genesis, proposalId, nodeB.ownerKey,
                             nodeB.validatorKey.address().value(),
                             core::GovernanceVoteChoice::YES, 1, unixTime());
    const core::Transaction voteC =
        signedGovernanceVote(genesis, proposalId, nodeC.ownerKey,
                             nodeC.validatorKey.address().value(),
                             core::GovernanceVoteChoice::NO, 1, unixTime());

    const auto submittedVoteA = submitTransaction(nodeA, voteA);
    require(submittedVoteA.has_value() && submittedVoteA->statusCode == 200 &&
                submittedVoteA->body.find("\"status\":\"ACCEPTED\"") !=
                    std::string::npos,
            "Validator A's vote was not accepted: " +
                (submittedVoteA.has_value() ? submittedVoteA->body : ""));

    const auto submittedVoteB = submitTransaction(nodeB, voteB);
    require(submittedVoteB.has_value() && submittedVoteB->statusCode == 200 &&
                submittedVoteB->body.find("\"status\":\"ACCEPTED\"") !=
                    std::string::npos,
            "Validator B's vote (submitted at node B, on a proposal from "
            "node A) was not accepted: " +
                (submittedVoteB.has_value() ? submittedVoteB->body : ""));

    const auto submittedVoteC = submitTransaction(nodeC, voteC);
    require(submittedVoteC.has_value() && submittedVoteC->statusCode == 200 &&
                submittedVoteC->body.find("\"status\":\"ACCEPTED\"") !=
                    std::string::npos,
            "Validator C's vote was not accepted: " +
                (submittedVoteC.has_value() ? submittedVoteC->body : ""));

    // Step 4: once every vote is finalized, the tally must be identical on
    // all 3 nodes: 2 YES (weight 1000 each) + 1 NO (weight 1000). Each vote
    // was submitted to a different node, so all 3 need to both produce/
    // receive the finalizing block and gossip it to the other two before
    // they agree.
    const auto tallyShowsAllVotes = [&](const NodeSpec &spec) {
      const auto tally = governanceTally(spec, proposalId);
      return tally.has_value() &&
             tally->body.find("\"participatingWeight\":3000") !=
                 std::string::npos;
    };
    require(waitUntil(60s,
                      [&] {
                        return tallyShowsAllVotes(nodeA) &&
                               tallyShowsAllVotes(nodeB) &&
                               tallyShowsAllVotes(nodeC);
                      }),
            "Votes did not finalize identically on all 3 nodes within the "
            "voting period.");

    const auto tallyA = governanceTally(nodeA, proposalId);
    const auto tallyB = governanceTally(nodeB, proposalId);
    const auto tallyC = governanceTally(nodeC, proposalId);
    require(tallyA.has_value() && tallyB.has_value() && tallyC.has_value(),
            "Unable to query governance tally from all 3 nodes.");
    require(tallyA->body == tallyB->body && tallyB->body == tallyC->body,
            "Governance tally differs across nodes:\nA: " + tallyA->body +
                "\nB: " + tallyB->body + "\nC: " + tallyC->body);
    require(tallyA->body.find("\"yesWeight\":2000") != std::string::npos &&
                tallyA->body.find("\"noWeight\":1000") != std::string::npos &&
                tallyA->body.find("\"totalEligibleWeight\":3000") !=
                    std::string::npos &&
                tallyA->body.find("\"quorumMet\":true") != std::string::npos &&
                tallyA->body.find("\"approvalThresholdMet\":true") !=
                    std::string::npos,
            "Governance tally does not reflect the expected weighted votes: " +
                tallyA->body);

    // Step 5: the same validator voting again must be rejected — defense in
    // depth. Submitted while the proposal is still open for voting, so the
    // rejection specifically exercises the duplicate-vote check (the
    // governance executor already has a recorded vote for this validator),
    // not merely the voting-period-closed check.
    const core::Transaction duplicateVoteB =
        signedGovernanceVote(genesis, proposalId, nodeB.ownerKey,
                             nodeB.validatorKey.address().value(),
                             core::GovernanceVoteChoice::NO, 2, unixTime());
    const auto duplicateSubmission = submitTransaction(nodeC, duplicateVoteB);
    require(
        duplicateSubmission.has_value() &&
            duplicateSubmission->body.find("\"status\":\"ACCEPTED\"") ==
                std::string::npos,
        "A duplicate vote from an already-voted validator must be "
        "rejected, but was accepted: " +
            (duplicateSubmission.has_value() ? duplicateSubmission->body : ""));

    // The rejected resubmission must not have moved the tally at all.
    const auto tallyAfterDuplicate = governanceTally(nodeC, proposalId);
    require(
        tallyAfterDuplicate.has_value() &&
            tallyAfterDuplicate->body == tallyC->body,
        "Tally changed after a rejected duplicate vote: " +
            (tallyAfterDuplicate.has_value() ? tallyAfterDuplicate->body : ""));

    // The proposal itself (status, decision) must also converge identically
    // once its voting period ends. Nothing else is happening on this chain
    // right now, so nothing will finalize a block on its own (see
    // driveChainUntil) — keep feeding it filler transactions until enough
    // blocks have passed for the voting period to close.
    std::uint64_t fillerNonce = 5;
    bool executed = false;
    driveChainUntil(nodeA, genesis, seedPrefix, fillerNonce,
                    /*maxFillerBlocks=*/60, [&] {
                      const auto response =
                          httpRequest(nodeA.rpcPort, "GET",
                                      "/governance/proposal/" + proposalId);
                      executed =
                          response.has_value() &&
                          response->body.find("\"status\":\"EXECUTED\"") !=
                              std::string::npos;
                      return executed;
                    });
    require(executed,
            "Governance proposal did not reach EXECUTED on node A within "
            "its voting period.");

    // Give nodes B and C a brief moment to receive and finalize the same
    // decision-bearing block via gossip + consensus before comparing.
    require(
        waitUntil(30s,
                  [&] {
                    const auto responseB =
                        httpRequest(nodeB.rpcPort, "GET",
                                    "/governance/proposal/" + proposalId);
                    const auto responseC =
                        httpRequest(nodeC.rpcPort, "GET",
                                    "/governance/proposal/" + proposalId);
                    return responseB.has_value() &&
                           responseB->body.find("\"status\":\"EXECUTED\"") !=
                               std::string::npos &&
                           responseC.has_value() &&
                           responseC->body.find("\"status\":\"EXECUTED\"") !=
                               std::string::npos;
                  }),
        "Governance proposal reached EXECUTED on node A but not on "
        "nodes B and C.");

    const auto proposalA =
        httpRequest(nodeA.rpcPort, "GET", "/governance/proposal/" + proposalId);
    const auto proposalB =
        httpRequest(nodeB.rpcPort, "GET", "/governance/proposal/" + proposalId);
    const auto proposalC =
        httpRequest(nodeC.rpcPort, "GET", "/governance/proposal/" + proposalId);
    require(proposalA.has_value() && proposalB.has_value() &&
                proposalC.has_value(),
            "Unable to query governance proposal from all 3 nodes.");
    require(proposalA->body == proposalB->body &&
                proposalB->body == proposalC->body,
            "Governance proposal decision differs across nodes:\nA: " +
                proposalA->body + "\nB: " + proposalB->body +
                "\nC: " + proposalC->body);

    nodes.stopAll();
    std::filesystem::remove_all(root, cleanupError);
  } catch (...) {
    nodes.stopAll();
    std::filesystem::remove_all(root, cleanupError);
    throw;
  }
}

#endif

} // namespace

int main() {
#ifdef _WIN32
  std::cout << "Governance gossip E2E test requires POSIX process control.\n";
  return 0;
#else
  try {
    testGovernanceProposalAndVoteGossip();
    std::cout << "Governance gossip end-to-end test passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Governance gossip end-to-end test FAILED: " << error.what()
              << '\n';
    return 1;
  }
#endif
}

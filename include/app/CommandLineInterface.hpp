#ifndef NODO_APP_COMMAND_LINE_INTERFACE_HPP
#define NODO_APP_COMMAND_LINE_INTERFACE_HPP

#include "config/NetworkParameters.hpp"
#include "p2p/PeerMessage.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>


namespace nodo::app {

class CommandLineOptions {
public:
    CommandLineOptions();

    std::string command;
    std::filesystem::path dataDirectory;
    std::string networkName;
    std::string peerId;
    std::string endpoint;
    std::string listenAddress;          // --listen HOST:PORT for node run
    std::vector<std::string> peers;     // --peer NAME@HOST:PORT (repeatable)
    std::string keyId;
    std::string validatorKeyId;
    std::string keyType;
    std::string toAddress;
    std::string validatorAddress;
    std::string governanceProposalId;
    std::string governanceProposalType;
    std::string governanceProposalTitle;
    std::string governanceProposalBody;
    std::string governanceTarget;
    std::string governanceValue;
    std::string governanceVoteChoice;
    std::int64_t amountRaw;
    std::int64_t feeRaw;
    std::uint64_t nonce;
    std::int64_t timestamp;
    std::uint64_t governanceEffectiveHeight;
    std::uint64_t governanceVotingPeriodBlocks;
    bool showHelp;
    bool keyIdProvided;
    bool validatorKeyIdProvided;
};

enum class CommandLineStatus {
    SUCCESS,
    INVALID_ARGUMENTS,
    COMMAND_FAILED
};

std::string commandLineStatusToString(
    CommandLineStatus status
);

class CommandLineResult {
public:
    CommandLineResult();

    static CommandLineResult success(
        std::string message
    );

    static CommandLineResult failure(
        CommandLineStatus status,
        std::string message
    );

    CommandLineStatus status() const;
    const std::string& message() const;
    bool success() const;

private:
    CommandLineStatus m_status;
    std::string m_message;
};

class CommandLineInterface {
public:
    static int run(
        int argc,
        char** argv
    );

    static CommandLineResult execute(
        const std::vector<std::string>& args
    );

    static CommandLineOptions parse(
        const std::vector<std::string>& args
    );

    static std::string helpText();

    static std::string defaultLocalnetKeyId();
    static std::string defaultLocalnetKeySeed();
    static p2p::PeerInfo localPeerFromOptions(
        const CommandLineOptions& options
    );

private:
    static CommandLineResult executeInit(
        const CommandLineOptions& options
    );

    static CommandLineResult executeStatus(
        const CommandLineOptions& options
    );

    static CommandLineResult executeInspect(
        const CommandLineOptions& options
    );

    static CommandLineResult executeReload(
        const CommandLineOptions& options
    );

    static CommandLineResult executeChainAudit(
        const CommandLineOptions& options
    );

    static CommandLineResult executeTestnetReadiness(
        const CommandLineOptions& options
    );

    static CommandLineResult executeDiagnostics(
        const CommandLineOptions& options
    );

    static CommandLineResult executeKeysCreate(
        const CommandLineOptions& options
    );

    static CommandLineResult executeKeysList(
        const CommandLineOptions& options
    );

    static CommandLineResult executeValidatorList(
        const CommandLineOptions& options
    );

    static CommandLineResult executeValidatorStatus(
        const CommandLineOptions& options
    );

    static CommandLineResult executeValidatorExit(
        const CommandLineOptions& options
    );

    static CommandLineResult executeValidatorUnjail(
        const CommandLineOptions& options
    );

    static CommandLineResult executeStakeLock(
        const CommandLineOptions& options
    );

    static CommandLineResult executeStakeStatus(
        const CommandLineOptions& options
    );

    static CommandLineResult executeStakePositions(
        const CommandLineOptions& options
    );

    static CommandLineResult executeStakeAudit(
        const CommandLineOptions& options
    );

    static CommandLineResult executeRewardsStatus(
        const CommandLineOptions& options
    );

    static CommandLineResult executeSlashingEvidence(
        const CommandLineOptions& options
    );

    static CommandLineResult executeProduceBlock(
        const CommandLineOptions& options
    );

    static CommandLineResult executeSubmitTransaction(
        const CommandLineOptions& options
    );

    static CommandLineResult executeGovernancePropose(
        const CommandLineOptions& options
    );

    static CommandLineResult executeGovernanceVote(
        const CommandLineOptions& options
    );

    static CommandLineResult executeGovernanceStatus(
        const CommandLineOptions& options
    );

    static CommandLineResult executeGovernanceList(
        const CommandLineOptions& options
    );

    static CommandLineResult executeGovernanceShow(
        const CommandLineOptions& options
    );

    static CommandLineResult executeGovernanceAudit(
        const CommandLineOptions& options
    );

    static CommandLineResult executeNodeRun(
        const CommandLineOptions& options
    );
};

} // namespace nodo::app

#endif

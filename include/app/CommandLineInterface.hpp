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
    std::string peerId;
    std::string endpoint;
    std::int64_t timestamp;
    bool showHelp;
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

    static config::GenesisConfig developmentGenesisConfig();
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

    static CommandLineResult executeProduceDemoBlock(
        const CommandLineOptions& options
    );
};

} // namespace nodo::app

#endif

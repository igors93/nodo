#ifndef NODO_NODE_JSON_RPC_SERVER_HPP
#define NODO_NODE_JSON_RPC_SERVER_HPP

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * JsonRpcRequest holds a parsed JSON-RPC 2.0 request.
 *
 * Security principle:
 * Parsing is defensive: any missing or unexpected field causes the request to
 * be rejected with an appropriate JSON-RPC error code.
 */
struct JsonRpcRequest {
    std::string jsonrpc;  // must be "2.0"
    std::string method;
    std::string params;   // raw JSON params string
    std::string id;       // string id for correlation

    bool isValid() const;
    static JsonRpcRequest parse(const std::string& rawJson);
};

struct JsonRpcResponse {
    std::string jsonrpc{"2.0"};
    std::string id;
    std::string result;   // JSON result string (set on success)
    std::string error;    // JSON error object string (set on failure)

    bool isSuccess() const;
    std::string serialize() const;

    static JsonRpcResponse success(const std::string& id, const std::string& result);
    static JsonRpcResponse makeError(const std::string& id, int code, const std::string& message);
};

// Standard JSON-RPC 2.0 error codes
struct JsonRpcError {
    static constexpr int PARSE_ERROR      = -32700;
    static constexpr int INVALID_REQUEST  = -32600;
    static constexpr int METHOD_NOT_FOUND = -32601;
    static constexpr int INVALID_PARAMS   = -32602;
    static constexpr int INTERNAL_ERROR   = -32603;
};

/*
 * JsonRpcDispatcher routes JSON-RPC 2.0 requests to registered handler functions.
 *
 * Supported standard nodo_* methods (registered via registerStandardMethods):
 *   nodo_getBlockByHeight      params: {"height": N}
 *   nodo_getBlockByHash        params: {"hash": "0x..."}
 *   nodo_getTransactionById    params: {"id": "..."}
 *   nodo_getAccountState       params: {"address": "..."}
 *   nodo_sendTransaction       params: {"tx": "<serialized tx>"}
 *   nodo_getMempoolStats       params: {}
 *   nodo_estimateFee           params: {"urgency": "LOW"|"MEDIUM"|"HIGH"}
 *   nodo_getChainInfo          params: {}
 *   nodo_getValidators         params: {}
 *   stake_status               params: {"validator": "..."}
 *   stake_positions            params: {"address": "..."} (address optional)
 *   stake_getPosition          params: {"positionId": "..."}
 *   stake_deposit/topUp/unlock/withdraw params: {"transaction": "<serialized signed tx>"}
 *   stake_pendingUnbonding     params: {"validator": "..."}
 *   stake_validatorStake       params: {"validator": "..."}
 *   stake_auditStatus          params: {}
 */
class JsonRpcDispatcher {
public:
    using Handler = std::function<JsonRpcResponse(const JsonRpcRequest&)>;

    JsonRpcDispatcher();

    void registerHandler(const std::string& method, Handler handler);

    JsonRpcResponse dispatch(const std::string& rawJson) const;

    // Register all standard nodo_* methods using the provided state accessors.
    void registerStandardMethods(
        std::function<std::string(std::uint64_t)>        getBlockByHeight,
        std::function<std::string(const std::string&)>   getBlockByHash,
        std::function<std::string(const std::string&)>   getTransactionById,
        std::function<std::string(const std::string&)>   getAccountState,
        std::function<std::string(const std::string&)>   sendTransaction,
        std::function<std::string()>                     getMempoolStats,
        std::function<std::string(const std::string&)>   estimateFee,
        std::function<std::string()>                     getChainInfo,
        std::function<std::string()>                     getValidators
    );

    void registerGovernanceMethods(
        std::function<std::string()> governanceProposals,
        std::function<std::string(const std::string&)> governanceGetProposal,
        std::function<std::string(const std::string&)> governanceGetVotes,
        std::function<std::string(const std::string&)> governanceGetTally,
        std::function<std::string(const std::string&)> governanceGetDecision,
        std::function<std::string(const std::string&)> governanceGetExecution,
        std::function<std::string(const std::string&)> governanceSubmitProposal,
        std::function<std::string(const std::string&)> governanceSubmitVote,
        std::function<std::string()> governanceStatus
    );

    void registerStakingMethods(
        std::function<std::string(const std::string&)> stakeStatus,
        std::function<std::string(const std::string&)> stakePositions,
        std::function<std::string(const std::string&)> stakeGetPosition,
        std::function<std::string(const std::string&)> stakeSubmitSignedTransaction,
        std::function<std::string(const std::string&)> stakePendingUnbonding,
        std::function<std::string(const std::string&)> stakeValidatorStake,
        std::function<std::string()> stakeAuditStatus
    );

    std::vector<std::string> registeredMethods() const;

private:
    std::map<std::string, Handler> m_handlers;

    static std::string extractParam(const std::string& paramsJson, const std::string& key);
};

} // namespace nodo::node

#endif

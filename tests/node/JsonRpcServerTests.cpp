#include "node/JsonRpcServer.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::node::JsonRpcDispatcher;
using nodo::node::JsonRpcError;
using nodo::node::JsonRpcRequest;
using nodo::node::JsonRpcResponse;

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testDispatchMethodNotFound() {
    JsonRpcDispatcher dispatcher;

    const auto response = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"nodo_nonexistent","params":{},"id":"1"})"
    );

    requireCondition(
        !response.isSuccess(),
        "dispatch should return error response for unknown method."
    );

    requireCondition(
        response.error.find("-32601") != std::string::npos,
        "Error response should contain METHOD_NOT_FOUND code -32601."
    );
}

void testDispatchParseErrorForMalformedJson() {
    JsonRpcDispatcher dispatcher;

    // Not valid JSON
    const auto response = dispatcher.dispatch("this is not json at all!!!");

    requireCondition(
        !response.isSuccess(),
        "dispatch should return error for malformed JSON."
    );

    requireCondition(
        response.error.find("-32700") != std::string::npos ||
        response.error.find("-32600") != std::string::npos,
        "Error response should contain PARSE_ERROR or INVALID_REQUEST code."
    );
}

void testRegisteredHandlerIsCalled() {
    JsonRpcDispatcher dispatcher;

    bool handlerCalled = false;

    dispatcher.registerHandler(
        "test_ping",
        [&handlerCalled](const JsonRpcRequest& req) -> JsonRpcResponse {
            handlerCalled = true;
            return JsonRpcResponse::success(req.id, "\"pong\"");
        }
    );

    const auto response = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"test_ping","params":{},"id":"42"})"
    );

    requireCondition(
        handlerCalled,
        "Registered handler should be called when method is dispatched."
    );

    requireCondition(
        response.isSuccess(),
        "Response should indicate success when handler returns success."
    );

    requireCondition(
        response.result == "\"pong\"",
        "Response result should match what the handler returned."
    );
}

void testSuccessResponseHasCorrectFields() {
    const auto response = JsonRpcResponse::success("req-id-99", "{\"height\":42}");

    requireCondition(
        response.jsonrpc == "2.0",
        "Response jsonrpc field should be 2.0."
    );

    requireCondition(
        response.id == "req-id-99",
        "Response id should match the request id."
    );

    requireCondition(
        response.isSuccess(),
        "Response should be marked as success."
    );

    const std::string serialized = response.serialize();
    requireCondition(
        serialized.find("\"jsonrpc\"") != std::string::npos,
        "Serialized response should contain jsonrpc field."
    );

    requireCondition(
        serialized.find("\"result\"") != std::string::npos,
        "Serialized success response should contain result field."
    );
}

void testErrorResponseHasCorrectCode() {
    const auto response = JsonRpcResponse::makeError(
        "req-id-7",
        JsonRpcError::INTERNAL_ERROR,
        "Something went wrong"
    );

    requireCondition(
        !response.isSuccess(),
        "Error response should not be marked as success."
    );

    requireCondition(
        response.error.find("-32603") != std::string::npos,
        "Error response should contain INTERNAL_ERROR code -32603."
    );

    requireCondition(
        response.id == "req-id-7",
        "Error response id should match the request id."
    );

    const std::string serialized = response.serialize();
    requireCondition(
        serialized.find("\"error\"") != std::string::npos,
        "Serialized error response should contain error field."
    );
}

void testGovernanceMethodsAreRegisteredAndDispatch() {
    JsonRpcDispatcher dispatcher;

    dispatcher.registerGovernanceMethods(
        []() { return R"({"proposals":["p1"]})"; },
        [](const std::string& id) { return "{\"proposalId\":\"" + id + "\"}"; },
        [](const std::string& id) { return "{\"votes\":\"" + id + "\"}"; },
        [](const std::string& id) { return "{\"tally\":\"" + id + "\"}"; },
        [](const std::string& id) { return "{\"decision\":\"" + id + "\"}"; },
        [](const std::string& id) { return "{\"execution\":\"" + id + "\"}"; },
        [](const std::string& tx) { return "{\"proposalTx\":\"" + tx + "\"}"; },
        [](const std::string& tx) { return "{\"voteTx\":\"" + tx + "\"}"; },
        []() { return R"({"activeProposalCount":1})"; }
    );

    const auto proposal = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"governance_getProposal","params":{"proposalId":"p1"},"id":"7"})"
    );

    requireCondition(
        proposal.isSuccess() &&
        proposal.result.find("\"proposalId\":\"p1\"") != std::string::npos,
        "governance_getProposal should dispatch to the registered callback."
    );

    const auto missing = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"governance_getTally","params":{},"id":"8"})"
    );

    requireCondition(
        !missing.isSuccess() &&
        missing.error.find("-32602") != std::string::npos,
        "Governance proposal-id methods should reject missing proposalId."
    );

    const auto submitVote = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"governance_submitVote","params":{"tx":"signed-vote"},"id":"9"})"
    );

    requireCondition(
        submitVote.isSuccess() &&
        submitVote.result.find("\"voteTx\":\"signed-vote\"") != std::string::npos,
        "governance_submitVote should dispatch signed transaction payloads."
    );
}

} // namespace

int main() {
    try {
        testDispatchMethodNotFound();
        testDispatchParseErrorForMalformedJson();
        testRegisteredHandlerIsCalled();
        testSuccessResponseHasCorrectFields();
        testErrorResponseHasCorrectCode();
        testGovernanceMethodsAreRegisteredAndDispatch();

        std::cout << "Nodo JsonRpcServer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo JsonRpcServer tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}

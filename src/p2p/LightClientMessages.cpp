#include "p2p/LightClientMessages.hpp"

#include <sstream>

namespace nodo::p2p {

namespace {

// Extract the value for a key in a "key=value;key=value" flat string.
// Returns empty string if the key is not found.
std::string extractField(const std::string& s, const std::string& key) {
    const std::string search = key + "=";
    const std::size_t pos = s.find(search);
    if (pos == std::string::npos) {
        return "";
    }

    const std::size_t valueStart = pos + search.size();
    const std::size_t valueEnd = s.find(';', valueStart);

    if (valueEnd == std::string::npos) {
        // Value runs to end of string (or closing brace)
        std::size_t end = s.find('}', valueStart);
        if (end == std::string::npos) {
            end = s.size();
        }
        return s.substr(valueStart, end - valueStart);
    }

    return s.substr(valueStart, valueEnd - valueStart);
}

} // namespace

// ---------------------------------------------------------------------------
// LightClientProofRequest
// ---------------------------------------------------------------------------

bool LightClientProofRequest::isValid() const {
    if (requestId.empty() || blockHeight == 0) {
        return false;
    }
    if (isTransactionRequest) {
        return !transactionId.empty();
    }
    return !address.empty();
}

std::string LightClientProofRequest::serialize() const {
    std::ostringstream oss;
    oss << "LightClientProofRequest{"
        << "requestId=" << requestId
        << ";transactionId=" << transactionId
        << ";address=" << address
        << ";blockHeight=" << blockHeight
        << ";isTransactionRequest=" << (isTransactionRequest ? "true" : "false")
        << "}";
    return oss.str();
}

LightClientProofRequest LightClientProofRequest::deserialize(const std::string& s) {
    LightClientProofRequest req;
    req.requestId          = extractField(s, "requestId");
    req.transactionId      = extractField(s, "transactionId");
    req.address            = extractField(s, "address");

    const std::string heightStr = extractField(s, "blockHeight");
    req.blockHeight = heightStr.empty() ? 0 : std::stoull(heightStr);

    const std::string isTxStr = extractField(s, "isTransactionRequest");
    req.isTransactionRequest = (isTxStr == "true");

    return req;
}

// ---------------------------------------------------------------------------
// LightClientProofResponse
// ---------------------------------------------------------------------------

bool LightClientProofResponse::isValid() const {
    if (requestId.empty()) {
        return false;
    }
    if (found && proofPayload.empty()) {
        return false;
    }
    return true;
}

std::string LightClientProofResponse::serialize() const {
    std::ostringstream oss;
    oss << "LightClientProofResponse{"
        << "requestId=" << requestId
        << ";found=" << (found ? "true" : "false")
        << ";reason=" << reason
        << ";proofPayload=" << proofPayload
        << "}";
    return oss.str();
}

LightClientProofResponse LightClientProofResponse::deserialize(const std::string& s) {
    LightClientProofResponse resp;
    resp.requestId    = extractField(s, "requestId");
    resp.reason       = extractField(s, "reason");
    resp.proofPayload = extractField(s, "proofPayload");

    const std::string foundStr = extractField(s, "found");
    resp.found = (foundStr == "true");

    return resp;
}

} // namespace nodo::p2p

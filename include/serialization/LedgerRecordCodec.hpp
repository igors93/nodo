#ifndef NODO_SERIALIZATION_LEDGER_RECORD_CODEC_HPP
#define NODO_SERIALIZATION_LEDGER_RECORD_CODEC_HPP

#include "core/LedgerRecord.hpp"

#include <string>
#include <vector>

namespace nodo::serialization {

/*
 * LedgerRecordCodec centralizes safe reconstruction of LedgerRecord objects
 * from deterministic text serialization.
 *
 * Security principle:
 * Parsing critical ledger records must happen through one audited boundary.
 *
 * Current status:
 * This codec supports the current deterministic text format. Future versions
 * should migrate this boundary to a stricter canonical encoding.
 */
class LedgerRecordCodec {
public:
    static core::LedgerRecord deserialize(
        const std::string& serialized
    );

    static std::vector<core::LedgerRecord> deserializeList(
        const std::string& serializedList
    );

    static std::vector<core::LedgerRecord> deserializeListFromBlockHeaderPayload(
        const std::string& blockHeaderPayload
    );

private:
    static core::LedgerRecordType parseLedgerRecordType(
        const std::string& value
    );

    static std::string extractPayload(
        const std::string& serialized
    );

    static void assertSafePayloadPrefixForType(
        core::LedgerRecordType type,
        const std::string& payload
    );
};

} // namespace nodo::serialization

#endif

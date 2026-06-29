#include "node/FinalizedMonetarySectionCodec.hpp"

#include "economics/BurnRecord.hpp"
#include "economics/MintRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::uint64_t parseU64Strict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (const char current : value) {
        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const std::uint64_t parsed =
        static_cast<std::uint64_t>(
            std::stoull(
                value,
                &parsedSize
            )
        );

    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }

    return parsed;
}

std::size_t parseSizeStrict(
    const serialization::KeyValueFileDocument& document,
    const std::string& fieldName
) {
    const std::uint64_t parsed =
        parseU64Strict(
            document.requireField(fieldName),
            fieldName
        );

    if (parsed > static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max()) ||
        parsed > document.fields().size()) {
        throw std::invalid_argument(
            "Declared monetary record count exceeds document bounds: " +
            fieldName
        );
    }

    return static_cast<std::size_t>(parsed);
}

std::int64_t parseI64Strict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current = value[index];

        if (current == '-' && index == 0 && value.size() > 1) {
            continue;
        }

        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const std::int64_t parsed =
        static_cast<std::int64_t>(
            std::stoll(
                value,
                &parsedSize
            )
        );

    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }

    return parsed;
}

utils::Amount parseAmountStrict(
    const std::string& value,
    const std::string& fieldName
) {
    return utils::Amount::fromRawUnits(
        parseI64Strict(
            value,
            fieldName
        )
    );
}

void addMintRecordFields(
    std::set<std::string>& allowedFields,
    std::size_t index
) {
    const std::string prefix =
        "supplyDelta.mint." + std::to_string(index) + ".";

    allowedFields.insert(prefix + "id");
    allowedFields.insert(prefix + "authorizationId");
    allowedFields.insert(prefix + "recipientAddress");
    allowedFields.insert(prefix + "amountRawUnits");
    allowedFields.insert(prefix + "reason");
    allowedFields.insert(prefix + "epoch");
    allowedFields.insert(prefix + "sourceBlockIndex");
    allowedFields.insert(prefix + "sourceBlockHash");
    allowedFields.insert(prefix + "timestamp");
}

void addBurnRecordFields(
    std::set<std::string>& allowedFields,
    std::size_t index
) {
    const std::string prefix =
        "supplyDelta.burn." + std::to_string(index) + ".";

    allowedFields.insert(prefix + "id");
    allowedFields.insert(prefix + "blockHeight");
    allowedFields.insert(prefix + "epoch");
    allowedFields.insert(prefix + "sourceAddress");
    allowedFields.insert(prefix + "amountRawUnits");
    allowedFields.insert(prefix + "reason");
    allowedFields.insert(prefix + "burnType");
}

} // namespace

std::size_t FinalizedMonetarySectionCodec::mintRecordCount(
    const serialization::KeyValueFileDocument& document
) {
    return parseSizeStrict(
        document,
        "supplyDelta.mintRecordCount"
    );
}

std::size_t FinalizedMonetarySectionCodec::burnRecordCount(
    const serialization::KeyValueFileDocument& document
) {
    return parseSizeStrict(
        document,
        "supplyDelta.burnRecordCount"
    );
}

void FinalizedMonetarySectionCodec::addAllowedFields(
    std::set<std::string>& allowedFields,
    std::size_t mintRecordCount,
    std::size_t burnRecordCount
) {
    allowedFields.insert("supplyDelta.blockHeight");
    allowedFields.insert("supplyDelta.blockHash");
    allowedFields.insert("supplyDelta.epoch");
    allowedFields.insert("supplyDelta.supplyBeforeRawUnits");
    allowedFields.insert("supplyDelta.mintedAmountRawUnits");
    allowedFields.insert("supplyDelta.burnedAmountRawUnits");
    allowedFields.insert("supplyDelta.supplyAfterRawUnits");
    allowedFields.insert("supplyDelta.mintRecordCount");
    allowedFields.insert("supplyDelta.burnRecordCount");

    for (std::size_t index = 0; index < mintRecordCount; ++index) {
        addMintRecordFields(
            allowedFields,
            index
        );
    }

    for (std::size_t index = 0; index < burnRecordCount; ++index) {
        addBurnRecordFields(
            allowedFields,
            index
        );
    }
}

economics::SupplyDelta FinalizedMonetarySectionCodec::decodeSupplyDelta(
    const serialization::KeyValueFileDocument& document,
    std::uint64_t expectedBlockHeight,
    const std::string& expectedBlockHash
) {
    const std::uint64_t blockHeight =
        parseU64Strict(
            document.requireField("supplyDelta.blockHeight"),
            "supplyDelta.blockHeight"
        );

    const std::string blockHash =
        document.requireField("supplyDelta.blockHash");

    if (blockHeight != expectedBlockHeight ||
        blockHash != expectedBlockHash) {
        throw std::invalid_argument("Finalized supply delta does not match block identity.");
    }

    const std::size_t mintCount =
        mintRecordCount(document);

    const std::size_t burnCount =
        burnRecordCount(document);

    std::vector<economics::MintRecord> mintRecords;
    mintRecords.reserve(mintCount);

    for (std::size_t index = 0; index < mintCount; ++index) {
        const std::string prefix =
            "supplyDelta.mint." + std::to_string(index) + ".";

        mintRecords.emplace_back(
            document.requireField(prefix + "id"),
            document.requireField(prefix + "authorizationId"),
            document.requireField(prefix + "recipientAddress"),
            parseAmountStrict(
                document.requireField(prefix + "amountRawUnits"),
                prefix + "amountRawUnits"
            ),
            economics::mintReasonFromString(
                document.requireField(prefix + "reason")
            ),
            parseU64Strict(
                document.requireField(prefix + "epoch"),
                prefix + "epoch"
            ),
            parseU64Strict(
                document.requireField(prefix + "sourceBlockIndex"),
                prefix + "sourceBlockIndex"
            ),
            document.requireField(prefix + "sourceBlockHash"),
            parseI64Strict(
                document.requireField(prefix + "timestamp"),
                prefix + "timestamp"
            )
        );
    }

    std::vector<economics::BurnRecord> burnRecords;
    burnRecords.reserve(burnCount);

    for (std::size_t index = 0; index < burnCount; ++index) {
        const std::string prefix =
            "supplyDelta.burn." + std::to_string(index) + ".";

        burnRecords.emplace_back(
            document.requireField(prefix + "id"),
            parseU64Strict(
                document.requireField(prefix + "blockHeight"),
                prefix + "blockHeight"
            ),
            parseU64Strict(
                document.requireField(prefix + "epoch"),
                prefix + "epoch"
            ),
            document.requireField(prefix + "sourceAddress"),
            parseAmountStrict(
                document.requireField(prefix + "amountRawUnits"),
                prefix + "amountRawUnits"
            ),
            document.requireField(prefix + "reason"),
            economics::burnTypeFromString(
                document.requireField(prefix + "burnType")
            )
        );
    }

    economics::SupplyDelta supplyDelta(
        blockHeight,
        blockHash,
        parseU64Strict(
            document.requireField("supplyDelta.epoch"),
            "supplyDelta.epoch"
        ),
        parseAmountStrict(
            document.requireField("supplyDelta.supplyBeforeRawUnits"),
            "supplyDelta.supplyBeforeRawUnits"
        ),
        parseAmountStrict(
            document.requireField("supplyDelta.mintedAmountRawUnits"),
            "supplyDelta.mintedAmountRawUnits"
        ),
        parseAmountStrict(
            document.requireField("supplyDelta.burnedAmountRawUnits"),
            "supplyDelta.burnedAmountRawUnits"
        ),
        parseAmountStrict(
            document.requireField("supplyDelta.supplyAfterRawUnits"),
            "supplyDelta.supplyAfterRawUnits"
        ),
        std::move(mintRecords),
        std::move(burnRecords)
    );

    if (!supplyDelta.isValid()) {
        throw std::invalid_argument(
            "Finalized supply delta is invalid: " + supplyDelta.rejectionReason()
        );
    }

    return supplyDelta;
}

void FinalizedMonetarySectionCodec::appendSupplyDeltaFields(
    const economics::SupplyDelta& supplyDelta,
    FieldList& fields
) {
    if (!supplyDelta.isValid()) {
        throw std::invalid_argument(
            "Cannot persist invalid supply delta: " + supplyDelta.rejectionReason()
        );
    }

    fields.emplace_back(
        "supplyDelta.blockHeight",
        std::to_string(supplyDelta.blockHeight())
    );
    fields.emplace_back(
        "supplyDelta.blockHash",
        supplyDelta.blockHash()
    );
    fields.emplace_back(
        "supplyDelta.epoch",
        std::to_string(supplyDelta.epoch())
    );
    fields.emplace_back(
        "supplyDelta.supplyBeforeRawUnits",
        std::to_string(supplyDelta.supplyBefore().rawUnits())
    );
    fields.emplace_back(
        "supplyDelta.mintedAmountRawUnits",
        std::to_string(supplyDelta.mintedAmount().rawUnits())
    );
    fields.emplace_back(
        "supplyDelta.burnedAmountRawUnits",
        std::to_string(supplyDelta.burnedAmount().rawUnits())
    );
    fields.emplace_back(
        "supplyDelta.supplyAfterRawUnits",
        std::to_string(supplyDelta.supplyAfter().rawUnits())
    );
    fields.emplace_back(
        "supplyDelta.mintRecordCount",
        std::to_string(supplyDelta.mintRecords().size())
    );
    fields.emplace_back(
        "supplyDelta.burnRecordCount",
        std::to_string(supplyDelta.burnRecords().size())
    );

    for (std::size_t index = 0; index < supplyDelta.mintRecords().size(); ++index) {
        const economics::MintRecord& mint = supplyDelta.mintRecords()[index];
        const std::string prefix =
            "supplyDelta.mint." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "id", mint.id());
        fields.emplace_back(prefix + "authorizationId", mint.authorizationId());
        fields.emplace_back(prefix + "recipientAddress", mint.recipientAddress());
        fields.emplace_back(
            prefix + "amountRawUnits",
            std::to_string(mint.amount().rawUnits())
        );
        fields.emplace_back(
            prefix + "reason",
            economics::mintReasonToString(mint.reason())
        );
        fields.emplace_back(prefix + "epoch", std::to_string(mint.epoch()));
        fields.emplace_back(
            prefix + "sourceBlockIndex",
            std::to_string(mint.sourceBlockIndex())
        );
        fields.emplace_back(prefix + "sourceBlockHash", mint.sourceBlockHash());
        fields.emplace_back(prefix + "timestamp", std::to_string(mint.timestamp()));
    }

    for (std::size_t index = 0; index < supplyDelta.burnRecords().size(); ++index) {
        const economics::BurnRecord& burn = supplyDelta.burnRecords()[index];
        const std::string prefix =
            "supplyDelta.burn." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "id", burn.burnId());
        fields.emplace_back(prefix + "blockHeight", std::to_string(burn.blockHeight()));
        fields.emplace_back(prefix + "epoch", std::to_string(burn.epoch()));
        fields.emplace_back(prefix + "sourceAddress", burn.sourceAddress());
        fields.emplace_back(
            prefix + "amountRawUnits",
            std::to_string(burn.amount().rawUnits())
        );
        fields.emplace_back(prefix + "reason", burn.reason());
        fields.emplace_back(
            prefix + "burnType",
            economics::burnTypeToString(burn.burnType())
        );
    }
}

} // namespace nodo::node

#include "core/CoinLotTransactionValidator.hpp"

#include "core/State.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::core {

namespace {

utils::Amount minAmount(
    const utils::Amount& left,
    const utils::Amount& right
) {
    return left <= right ? left : right;
}

} // namespace

CoinLotTransactionValidationResult CoinLotTransactionValidator::validateTransferTransaction(
    const Transaction& transaction,
    const CoinLotRegistry& registry
) {
    if (transaction.type() != TransactionType::TRANSFER) {
        return CoinLotTransactionValidationResult::invalid(
            "Only TRANSFER transactions can be validated against CoinLotRegistry."
        );
    }

    if (transaction.id().empty()) {
        return CoinLotTransactionValidationResult::invalid(
            "Transaction id is empty."
        );
    }

    if (transaction.fromAddress().empty() || transaction.toAddress().empty()) {
        return CoinLotTransactionValidationResult::invalid(
            "Transfer addresses cannot be empty."
        );
    }

    if (transaction.fromAddress() == transaction.toAddress()) {
        return CoinLotTransactionValidationResult::invalid(
            "Transfer sender and recipient cannot be the same."
        );
    }

    if (!transaction.amount().isPositive()) {
        return CoinLotTransactionValidationResult::invalid(
            "Transfer amount must be positive."
        );
    }

    if (transaction.fee().isNegative()) {
        return CoinLotTransactionValidationResult::invalid(
            "Transfer fee cannot be negative."
        );
    }

    if (transaction.nonce() == 0) {
        return CoinLotTransactionValidationResult::invalid(
            "Transfer nonce must be positive."
        );
    }

    if (transaction.timestamp() <= 0) {
        return CoinLotTransactionValidationResult::invalid(
            "Transfer timestamp must be positive."
        );
    }

    if (!registry.isValid()) {
        return CoinLotTransactionValidationResult::invalid(
            "CoinLotRegistry is invalid."
        );
    }

    const utils::Amount totalDebit =
        transaction.amount() + transaction.fee();

    if (!totalDebit.isPositive()) {
        return CoinLotTransactionValidationResult::invalid(
            "Transfer total debit must be positive."
        );
    }

    utils::Amount collected =
        utils::Amount::fromRawUnits(0);

    for (const auto& entry : registry.lots()) {
        const CoinLot& lot = entry.second;

        if (lot.ownerAddress() != transaction.fromAddress()) {
            continue;
        }

        if (!lot.isSpendable()) {
            continue;
        }

        collected = collected + lot.amount();

        if (collected >= totalDebit) {
            return CoinLotTransactionValidationResult::valid();
        }
    }

    return CoinLotTransactionValidationResult::invalid(
        "Insufficient spendable CoinLot balance for transfer."
    );
}

CoinLotTransferPlan CoinLotTransactionValidator::buildTransferPlan(
    const Transaction& transaction,
    const CoinLotRegistry& registry,
    std::uint64_t currentBlockIndex
) {
    const CoinLotTransactionValidationResult validation =
        validateTransferTransaction(
            transaction,
            registry
        );

    if (!validation.success()) {
        throw std::invalid_argument(
            "Cannot build CoinLotTransferPlan: " + validation.reason()
        );
    }

    const utils::Amount totalDebit =
        transaction.amount() + transaction.fee();

    std::vector<std::string> inputLotIds;
    std::vector<CoinLot> outputLots;

    utils::Amount collected =
        utils::Amount::fromRawUnits(0);

    utils::Amount remainingRecipientAmount =
        transaction.amount();

    utils::Amount remainingFeeAmount =
        transaction.fee();

    std::size_t outputIndex = 0;

    /*
     * Deterministic input selection:
     * CoinLotRegistry stores lots in std::map order by id. This keeps plan
     * generation stable across nodes that see the same registry.
     */
    for (const auto& entry : registry.lots()) {
        const CoinLot& inputLot = entry.second;

        if (inputLot.ownerAddress() != transaction.fromAddress()) {
            continue;
        }

        if (!inputLot.isSpendable()) {
            continue;
        }

        inputLotIds.push_back(inputLot.id());
        collected = collected + inputLot.amount();

        utils::Amount remainingInputAmount =
            inputLot.amount();

        auto createOutput = [&](const std::string& ownerAddress,
                                const utils::Amount& amount,
                                const std::string& outputKind) {
            if (!amount.isPositive()) {
                return;
            }

            const std::string outputLotId =
                createTransferOutputCoinLotId(
                    transaction,
                    inputLot,
                    outputKind,
                    outputIndex
                );

            if (outputIdAlreadyExistsInRegistryOrPending(
                    registry,
                    outputLots,
                    outputLotId
                )) {
                throw std::logic_error("Generated transfer output CoinLot id already exists.");
            }

            CoinLot outputLot(
                outputLotId,
                inputLot.originMintRecordId(),
                ownerAddress,
                amount,
                CoinLotStatus::AVAILABLE,
                currentBlockIndex,
                0,
                transaction.timestamp()
            );

            if (!outputLot.isValid()) {
                throw std::logic_error("Generated transfer output CoinLot is invalid.");
            }

            outputLots.push_back(outputLot);
            ++outputIndex;
        };

        while (remainingInputAmount.isPositive()) {
            if (remainingRecipientAmount.isPositive()) {
                const utils::Amount outputAmount =
                    minAmount(
                        remainingInputAmount,
                        remainingRecipientAmount
                    );

                createOutput(
                    transaction.toAddress(),
                    outputAmount,
                    "recipient"
                );

                remainingInputAmount = remainingInputAmount - outputAmount;
                remainingRecipientAmount = remainingRecipientAmount - outputAmount;
                continue;
            }

            if (remainingFeeAmount.isPositive()) {
                const utils::Amount outputAmount =
                    minAmount(
                        remainingInputAmount,
                        remainingFeeAmount
                    );

                createOutput(
                    State::feePoolAddress(),
                    outputAmount,
                    "fee"
                );

                remainingInputAmount = remainingInputAmount - outputAmount;
                remainingFeeAmount = remainingFeeAmount - outputAmount;
                continue;
            }

            createOutput(
                transaction.fromAddress(),
                remainingInputAmount,
                "change"
            );

            remainingInputAmount = utils::Amount::fromRawUnits(0);
        }

        if (collected >= totalDebit) {
            break;
        }
    }

    if (collected < totalDebit) {
        throw std::logic_error("CoinLotTransferPlan collected less than required debit.");
    }

    if (remainingRecipientAmount.isPositive() || remainingFeeAmount.isPositive()) {
        throw std::logic_error("CoinLotTransferPlan output allocation failed.");
    }

    const utils::Amount changeAmount =
        collected - totalDebit;

    CoinLotTransferPlan plan(
        transaction.id(),
        transaction.fromAddress(),
        transaction.toAddress(),
        inputLotIds,
        outputLots,
        transaction.amount(),
        transaction.fee(),
        changeAmount,
        collected
    );

    if (!plan.isValid()) {
        throw std::logic_error("Generated CoinLotTransferPlan is invalid.");
    }

    return plan;
}

void CoinLotTransactionValidator::applyTransfer(
    CoinLotRegistry& registry,
    const Transaction& transaction,
    std::uint64_t currentBlockIndex
) {
    const CoinLotTransferPlan plan =
        buildTransferPlan(
            transaction,
            registry,
            currentBlockIndex
        );

    /*
     * Validate all mutations before the first mutation is applied.
     * This keeps partial application risk low.
     */
    for (const auto& inputLotId : plan.inputLotIds()) {
        const CoinLotVerificationResult inputVerification =
            registry.verifySpendable(
                inputLotId,
                transaction.fromAddress()
            );

        if (!inputVerification.success()) {
            throw std::invalid_argument(
                "Input CoinLot became invalid before transfer application: " +
                inputVerification.reason()
            );
        }
    }

    for (const auto& outputLot : plan.outputLots()) {
        if (!outputLot.isValid()) {
            throw std::logic_error("Transfer output lot is invalid before application.");
        }

        if (registry.hasLot(outputLot.id())) {
            throw std::logic_error("Transfer output lot already exists before application.");
        }
    }

    for (const auto& inputLotId : plan.inputLotIds()) {
        registry.markSpent(
            inputLotId,
            transaction.fromAddress()
        );
    }

    for (const auto& outputLot : plan.outputLots()) {
        registry.addLot(outputLot);
    }
}

std::string CoinLotTransactionValidator::createTransferOutputCoinLotId(
    const Transaction& transaction,
    const CoinLot& inputLot,
    const std::string& outputKind,
    std::size_t outputIndex
) {
    std::ostringstream oss;

    oss << "coinlot_from_tx_"
        << transaction.id()
        << "_input_"
        << inputLot.id()
        << "_"
        << outputKind
        << "_"
        << outputIndex;

    return oss.str();
}

bool CoinLotTransactionValidator::outputIdAlreadyExistsInRegistryOrPending(
    const CoinLotRegistry& registry,
    const std::vector<CoinLot>& pendingOutputs,
    const std::string& outputLotId
) {
    if (registry.hasLot(outputLotId)) {
        return true;
    }

    for (const auto& pendingOutput : pendingOutputs) {
        if (pendingOutput.id() == outputLotId) {
            return true;
        }
    }

    return false;
}

} // namespace nodo::core

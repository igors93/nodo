#include "storage/AccountStateSnapshotStore.hpp"

#include "core/AccountState.hpp"
#include "storage/AtomicFile.hpp"
#include "utils/Amount.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace nodo::storage {

AccountStateSnapshot::AccountStateSnapshot()
    : m_genesisConfigId("")
    , m_height(0)
    , m_blockHash("")
    , m_view()
{}

AccountStateSnapshot::AccountStateSnapshot(
    std::string genesisConfigId,
    std::uint64_t height,
    std::string blockHash,
    core::AccountStateView view
)
    : m_genesisConfigId(std::move(genesisConfigId))
    , m_height(height)
    , m_blockHash(std::move(blockHash))
    , m_view(std::move(view))
{}

const std::string& AccountStateSnapshot::genesisConfigId() const {
    return m_genesisConfigId;
}

std::uint64_t AccountStateSnapshot::height() const {
    return m_height;
}

const std::string& AccountStateSnapshot::blockHash() const {
    return m_blockHash;
}

const core::AccountStateView& AccountStateSnapshot::view() const {
    return m_view;
}

bool AccountStateSnapshot::isValid() const {
    return !m_genesisConfigId.empty() &&
           !m_blockHash.empty() &&
           m_view.isValid();
}

AccountStateSnapshotStore::AccountStateSnapshotStore(
    const std::filesystem::path& dataRoot
)
    : m_dataRoot(dataRoot)
{}

std::filesystem::path AccountStateSnapshotStore::snapshotPath() const {
    return m_dataRoot / kSnapshotFileName;
}

void AccountStateSnapshotStore::save(
    const AccountStateSnapshot& snapshot
) const {
    if (!snapshot.isValid()) return;

    std::ostringstream oss;
    oss << "NODO_ACCOUNT_SNAPSHOT_V1\n";
    oss << "genesisConfigId=" << snapshot.genesisConfigId() << "\n";
    oss << "height=" << snapshot.height() << "\n";
    oss << "blockHash=" << snapshot.blockHash() << "\n";
    for (const auto& account : snapshot.view().accounts()) {
        oss << account.address()
            << "\t" << account.balance().rawUnits()
            << "\t" << account.nonce()
            << "\n";
    }

    AtomicFile::writeTextFile(snapshotPath(), oss.str());
}

std::optional<AccountStateSnapshot> AccountStateSnapshotStore::load() const {
    const std::filesystem::path path = snapshotPath();
    if (!std::filesystem::exists(path)) return std::nullopt;

    std::string content;
    try {
        content = AtomicFile::readTextFile(path);
    } catch (...) {
        return std::nullopt;
    }

    std::istringstream iss(content);
    std::string line;

    if (!std::getline(iss, line) || line != "NODO_ACCOUNT_SNAPSHOT_V1") {
        return std::nullopt;
    }

    std::string genesisConfigId;
    std::uint64_t height = 0;
    std::string blockHash;

    auto parseField = [](const std::string& l, const std::string& key) -> std::string {
        if (l.rfind(key, 0) == 0) return l.substr(key.size());
        return {};
    };

    if (!std::getline(iss, line)) return std::nullopt;
    genesisConfigId = parseField(line, "genesisConfigId=");
    if (genesisConfigId.empty()) return std::nullopt;

    if (!std::getline(iss, line)) return std::nullopt;
    const std::string heightStr = parseField(line, "height=");
    if (heightStr.empty()) return std::nullopt;
    try { height = std::stoull(heightStr); } catch (...) { return std::nullopt; }

    if (!std::getline(iss, line)) return std::nullopt;
    blockHash = parseField(line, "blockHash=");
    if (blockHash.empty()) return std::nullopt;

    core::AccountStateView view;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        const auto tab1 = line.find('\t');
        if (tab1 == std::string::npos) return std::nullopt;
        const auto tab2 = line.find('\t', tab1 + 1);
        if (tab2 == std::string::npos) return std::nullopt;

        const std::string address = line.substr(0, tab1);
        std::int64_t balanceRaw = 0;
        std::uint64_t nonce = 0;
        try {
            balanceRaw = std::stoll(line.substr(tab1 + 1, tab2 - tab1 - 1));
            nonce = std::stoull(line.substr(tab2 + 1));
        } catch (...) {
            return std::nullopt;
        }
        view.putAccount(core::AccountState(address, utils::Amount(balanceRaw), nonce));
    }

    AccountStateSnapshot snapshot(genesisConfigId, height, blockHash, std::move(view));
    if (!snapshot.isValid()) return std::nullopt;
    return snapshot;
}

} // namespace nodo::storage

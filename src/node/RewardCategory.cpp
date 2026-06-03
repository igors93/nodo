#include "node/RewardCategory.hpp"

namespace nodo::node {

std::string rewardCategoryToString(RewardCategory category) {
    switch (category) {
        case RewardCategory::NORMAL_BLOCK:        return "NORMAL_BLOCK";
        case RewardCategory::PROTECTION:          return "PROTECTION";
        case RewardCategory::DEFERRED_PROTECTION: return "DEFERRED_PROTECTION";
        case RewardCategory::EXTRAORDINARY:       return "EXTRAORDINARY";
        case RewardCategory::REJECTED:            return "REJECTED";
        default:                                  return "UNKNOWN";
    }
}

bool rewardCategoryFromString(const std::string& s, RewardCategory& out) {
    if (s == "NORMAL_BLOCK") {
        out = RewardCategory::NORMAL_BLOCK;
        return true;
    }
    if (s == "PROTECTION") {
        out = RewardCategory::PROTECTION;
        return true;
    }
    if (s == "DEFERRED_PROTECTION") {
        out = RewardCategory::DEFERRED_PROTECTION;
        return true;
    }
    if (s == "EXTRAORDINARY") {
        out = RewardCategory::EXTRAORDINARY;
        return true;
    }
    if (s == "REJECTED") {
        out = RewardCategory::REJECTED;
        return true;
    }
    return false;
}

bool rewardCategoryRequiresWorkEvidence(RewardCategory category) {
    return category == RewardCategory::PROTECTION ||
           category == RewardCategory::DEFERRED_PROTECTION;
}

bool rewardCategoryBlockedInDefenseMode(RewardCategory category) {
    return category == RewardCategory::EXTRAORDINARY;
}

} // namespace nodo::node

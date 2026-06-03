#ifndef NODO_NODE_REWARD_CATEGORY_HPP
#define NODO_NODE_REWARD_CATEGORY_HPP

#include <string>

namespace nodo::node {

/*
 * RewardCategory classifies reward records so that monetary audit can verify
 * that protection rewards are evidence-backed and extraordinary rewards respect
 * defense mode policy.
 *
 * Categories:
 *   NORMAL_BLOCK       — standard per-block validator reward, no special evidence.
 *   PROTECTION         — reward for verified protection work (requires work evidence).
 *   DEFERRED_PROTECTION — protection reward delayed to a later settlement block.
 *   EXTRAORDINARY      — reward outside normal schedules (blocked when defense mode
 *                        policy requires; requires explicit authorization).
 *   REJECTED           — a reward that was computed but denied (e.g. failed audit,
 *                        defense mode block, or zero work score).
 */
enum class RewardCategory {
    NORMAL_BLOCK,
    PROTECTION,
    DEFERRED_PROTECTION,
    EXTRAORDINARY,
    REJECTED
};

std::string rewardCategoryToString(RewardCategory category);

bool rewardCategoryFromString(const std::string& s, RewardCategory& out);

// Returns true if this category requires verifiable protection work evidence.
bool rewardCategoryRequiresWorkEvidence(RewardCategory category);

// Returns true if this category is blocked when defense mode policy requires it.
bool rewardCategoryBlockedInDefenseMode(RewardCategory category);

} // namespace nodo::node

#endif

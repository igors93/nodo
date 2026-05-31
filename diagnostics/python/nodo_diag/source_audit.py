from __future__ import annotations

from dataclasses import dataclass, asdict
from pathlib import Path
import re


@dataclass
class SourceAudit:
    finalized_block_store_version: str | None
    runtime_state_loader_version: str | None
    persisted_fields_count: int
    allowed_fields_count: int
    canonical_fields_count: int
    persisted_not_allowed: list[str]
    persisted_not_canonical: list[str]
    governance_fields_present: bool
    protection_reward_fields_present: bool
    cryptographic_slashing_fields_present: bool
    fee_economics_fields_present: bool
    monetary_fields_present: bool

    def to_dict(self) -> dict:
        return asdict(self)


def read_text(path: Path) -> str:
    if not path.is_file():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def extract_version(text: str) -> str | None:
    match = re.search(r'NODO_FINALIZED_BLOCK_V\d+', text)
    return match.group(0) if match else None


def extract_persisted_fields(finalized_store_text: str) -> set[str]:
    fields: set[str] = set()

    for match in re.finditer(r'\{\s*"([^"]+)"\s*,', finalized_store_text):
        fields.add(match.group(1))

    for match in re.finditer(r'fields\.emplace_back\(\s*"([^"]+)"', finalized_store_text):
        fields.add(match.group(1))

    for match in re.finditer(r'fields\.emplace_back\(\s*prefix\s*\+\s*"([^"]+)"', finalized_store_text):
        fields.add("*." + match.group(1))

    return fields


def extract_allowed_fields(runtime_loader_text: str) -> set[str]:
    fields: set[str] = set()

    # Fields inside allowedFields = {"field", ...}
    allowed_blocks = re.findall(
        r'allowedFields\s*=\s*\{(.*?)\};',
        runtime_loader_text,
        flags=re.DOTALL,
    )

    for block in allowed_blocks:
        for field in re.findall(r'"([^"]+)"', block):
            fields.add(field)

    # Dynamic prefix fields.
    for match in re.finditer(r'allowedFields\.insert\(\s*prefix\s*\+\s*"([^"]+)"', runtime_loader_text):
        fields.add("*." + match.group(1))

    return fields


def extract_canonical_fields(runtime_loader_text: str) -> set[str]:
    fields: set[str] = set()

    for match in re.finditer(r'canonicalFields\.emplace_back\(\s*"([^"]+)"', runtime_loader_text):
        fields.add(match.group(1))

    for match in re.finditer(r'canonicalFields\.emplace_back\(\s*prefix\s*\+\s*"([^"]+)"', runtime_loader_text):
        fields.add("*." + match.group(1))

    for match in re.finditer(r'\{\s*"([^"]+)"\s*,', runtime_loader_text):
        fields.add(match.group(1))

    return fields


def has_prefixed_equivalent(field: str, fields: set[str]) -> bool:
    if "." not in field:
        return False

    suffix = field.split(".", 1)[1]
    return "*." + suffix in fields


def audit_sources(repo_root: Path) -> SourceAudit:
    finalized_store = read_text(repo_root / "src" / "node" / "FinalizedBlockStore.cpp")
    runtime_loader = read_text(repo_root / "src" / "node" / "RuntimeStateLoader.cpp")

    persisted = extract_persisted_fields(finalized_store)
    allowed = extract_allowed_fields(runtime_loader)
    canonical = extract_canonical_fields(runtime_loader)

    persisted_not_allowed = sorted(
        field for field in persisted
        if field not in allowed and not has_prefixed_equivalent(field, allowed)
    )

    persisted_not_canonical = sorted(
        field for field in persisted
        if field not in canonical and not has_prefixed_equivalent(field, canonical)
    )

    all_source = finalized_store + "\n" + runtime_loader

    return SourceAudit(
        finalized_block_store_version=extract_version(finalized_store),
        runtime_state_loader_version=extract_version(runtime_loader),
        persisted_fields_count=len(persisted),
        allowed_fields_count=len(allowed),
        canonical_fields_count=len(canonical),
        persisted_not_allowed=persisted_not_allowed,
        persisted_not_canonical=persisted_not_canonical,
        governance_fields_present="governancePolicyStatus" in all_source,
        protection_reward_fields_present="protectionRewardSummaryStatus" in all_source,
        cryptographic_slashing_fields_present="cryptographicSlashingSummaryStatus" in all_source,
        fee_economics_fields_present="feeEconomicBalanceStatus" in all_source,
        monetary_fields_present="monetaryFirewallStatus" in all_source,
    )

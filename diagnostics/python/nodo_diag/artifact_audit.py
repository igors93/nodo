from __future__ import annotations

from dataclasses import dataclass, asdict
from pathlib import Path
import re


@dataclass
class ArtifactFinding:
    path: str
    line: int
    key: str
    value: str
    problem: str

    def to_dict(self) -> dict:
        return asdict(self)


def parse_key_value_file(path: Path) -> dict[str, str]:
    fields: dict[str, str] = {}

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if "=" not in stripped:
            continue

        key, value = stripped.split("=", 1)
        fields[key] = value

    return fields


def find_candidate_artifacts(repo_root: Path) -> list[Path]:
    candidates: list[Path] = []
    artifact_suffixes = {".nodo", ".kv", ".block"}

    for path in repo_root.rglob("*"):
        if not path.is_file():
            continue

        if path.suffix not in artifact_suffixes:
            continue

        try:
            sample = path.read_text(encoding="utf-8", errors="replace")[:4096]
        except Exception:
            continue

        if "NODO_FINALIZED_BLOCK_" in sample or "monetaryFirewallStatus" in sample:
            candidates.append(path)

    return candidates[:200]


def audit_artifacts(repo_root: Path) -> list[ArtifactFinding]:
    findings: list[ArtifactFinding] = []

    required_groups = {
        "monetary": ["monetaryFirewallStatus", "monetary.reason"],
        "fee": ["feeEconomicBalanceStatus", "feeBalance.reason"],
        "protection_rewards": ["protectionRewardSummaryStatus", "protectionSummary.reason"],
        "cryptographic_slashing": [
            "cryptographicSlashingSummaryStatus",
            "cryptographicSlashingSummary.sourcePenaltyDigest",
        ],
        "governance": ["governancePolicyStatus", "governanceSummaryStatus"],
    }

    for artifact in find_candidate_artifacts(repo_root):
        lines = artifact.read_text(encoding="utf-8", errors="replace").splitlines()
        fields = parse_key_value_file(artifact)

        for line_number, line in enumerate(lines, start=1):
            if "=" not in line:
                continue
            key, value = line.split("=", 1)
            if value == "":
                findings.append(
                    ArtifactFinding(
                        path=str(artifact),
                        line=line_number,
                        key=key,
                        value=value,
                        problem="empty_value",
                    )
                )

        for group_name, keys in required_groups.items():
            if any(key in fields for key in keys):
                for key in keys:
                    if key not in fields:
                        findings.append(
                            ArtifactFinding(
                                path=str(artifact),
                                line=0,
                                key=key,
                                value="",
                                problem=f"missing_expected_{group_name}_field",
                            )
                        )

    return findings

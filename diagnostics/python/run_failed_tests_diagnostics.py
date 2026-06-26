#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from datetime import datetime, timezone

from nodo_diag.artifact_audit import audit_artifacts
from nodo_diag.ctest_runner import (
    classify_output,
    extract_failure_windows,
    find_ctest_build_dir,
    find_repo_root,
    focused_ctests,
    read_last_test_logs,
    run_failed_ctests,
)
from nodo_diag.source_audit import audit_sources


def make_markdown_report(report: dict) -> str:
    lines: list[str] = []

    lines.append("# Nodo test diagnostics")
    lines.append("")
    lines.append(f"Generated at: `{report['generated_at']}`")
    lines.append(f"Repository root: `{report['repo_root']}`")
    lines.append(f"Build directory: `{report['build_dir']}`")
    lines.append("")

    lines.append("## Focused test set")
    lines.append("")
    for test in report["focused_tests"]:
        lines.append(f"- `{test}`")
    lines.append("")

    source = report["source_audit"]

    lines.append("## Source audit")
    lines.append("")
    lines.append(f"- FinalizedBlockStore schema id: `{source['finalized_block_store_schema_id']}`")
    lines.append(f"- RuntimeStateLoader schema id: `{source['runtime_state_loader_schema_id']}`")
    lines.append(f"- Persisted fields: `{source['persisted_fields_count']}`")
    lines.append(f"- Loader allowed fields: `{source['allowed_fields_count']}`")
    lines.append(f"- Loader canonical fields: `{source['canonical_fields_count']}`")
    lines.append(f"- Governance fields present: `{source['governance_fields_present']}`")
    lines.append(
        f"- Protection reward fields present: `{source['protection_reward_fields_present']}`"
    )
    key = "cryptographic_slashing_fields_present"
    lines.append(f"- Cryptographic slashing fields present: `{source[key]}`")
    lines.append(f"- Fee economics fields present: `{source['fee_economics_fields_present']}`")
    lines.append(f"- Monetary fields present: `{source['monetary_fields_present']}`")
    lines.append("")

    if source["persisted_not_allowed"]:
        lines.append("### Persisted fields not accepted by RuntimeStateLoader")
        lines.append("")
        for field in source["persisted_not_allowed"]:
            lines.append(f"- `{field}`")
        lines.append("")

    if source["persisted_not_canonical"]:
        lines.append("### Persisted fields not recreated canonically by RuntimeStateLoader")
        lines.append("")
        for field in source["persisted_not_canonical"]:
            lines.append(f"- `{field}`")
        lines.append("")

    lines.append("## Artifact audit")
    lines.append("")
    if not report["artifact_findings"]:
        lines.append(
            "No finalized block artifact problems were found in files that still exist on disk."
        )
    else:
        for finding in report["artifact_findings"]:
            lines.append(
                f"- `{finding['problem']}` in `{finding['path']}` "
                f"line `{finding['line']}` key `{finding['key']}`"
            )
    lines.append("")

    lines.append("## ctest results")
    lines.append("")

    for result in report["ctest_results"]:
        command = " ".join(result["command"])
        lines.append(f"### `{command}`")
        lines.append("")
        lines.append(f"- Return code: `{result['returncode']}`")
        lines.append(f"- Duration: `{result['duration_seconds']}s`")
        lines.append(f"- Classifications: `{', '.join(result['classifications']) or 'none'}`")
        lines.append("")

        if result["failure_windows"]:
            lines.append("Failure windows:")
            lines.append("")
            for window in result["failure_windows"][:5]:
                lines.append("```text")
                lines.append(window[-4000:])
                lines.append("```")
                lines.append("")

    lines.append("## Recommended next checks")
    lines.append("")
    lines.append(
        "1. If a field appears in `persisted_not_allowed`, "
        "add it to RuntimeStateLoader allowed fields."
    )
    lines.append(
        "2. If a field appears in `persisted_not_canonical`, "
        "add it to RuntimeStateLoader canonical reconstruction."
    )
    lines.append(
        "3. If the failing output says `should persist`, "
        "compare FinalizedBlockStore fields with the test expectation."
    )
    lines.append(
        "4. If CLI tests fail after node tests, fix node persistence/reload first; "
        "CLI failures are often downstream."
    )
    lines.append("")

    return "\n".join(lines)


def main() -> int:
    repo_root = find_repo_root()
    build_dir = find_ctest_build_dir(repo_root)

    report_dir = repo_root / "diagnostics" / "reports"
    report_dir.mkdir(parents=True, exist_ok=True)

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")

    source_audit = audit_sources(repo_root)
    artifact_findings = audit_artifacts(repo_root)
    focused_tests = focused_ctests(build_dir)

    ctest_results = []

    for result in run_failed_ctests(build_dir, focused_tests):
        combined = result.stdout + "\n" + result.stderr
        data = result.to_dict()
        data["classifications"] = classify_output(combined) if result.returncode != 0 else []
        data["failure_windows"] = (
            extract_failure_windows(combined) if result.returncode != 0 else []
        )
        ctest_results.append(data)

    logs = read_last_test_logs(build_dir)

    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "repo_root": str(repo_root),
        "build_dir": str(build_dir),
        "focused_tests": focused_tests,
        "source_audit": source_audit.to_dict(),
        "artifact_findings": [finding.to_dict() for finding in artifact_findings],
        "ctest_results": ctest_results,
        "ctest_logs": logs,
    }

    json_path = report_dir / f"nodo_failure_diagnostics_{timestamp}.json"
    md_path = report_dir / f"nodo_failure_diagnostics_{timestamp}.md"

    json_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    md_path.write_text(make_markdown_report(report), encoding="utf-8")

    print("Diagnostic report written:")
    print(f"- {md_path}")
    print(f"- {json_path}")

    failed = [result for result in ctest_results if result["returncode"] != 0]

    if failed:
        print("")
        print(f"{len(failed)} focused tests are still failing.")
        print("Open the markdown report above and start with the first failure window.")
        return 1

    print("")
    print("All focused tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
import unittest
from datetime import datetime, timezone
from pathlib import Path


def find_repo_root(start: Path | None = None) -> Path:
    current = (start or Path.cwd()).resolve()

    for candidate in [current, *current.parents]:
        if (candidate / "CMakeLists.txt").is_file() and (candidate / "src").is_dir():
            return candidate

    raise RuntimeError(
        "Could not find Nodo repository root. Run this script from inside the Nodo repository."
    )


class JsonTestResult(unittest.TextTestResult):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.records: list[dict] = []

    def addSuccess(self, test):
        super().addSuccess(test)
        self.records.append(
            {
                "test": str(test),
                "status": "PASS",
                "details": "",
            }
        )

    def addFailure(self, test, err):
        super().addFailure(test, err)
        self.records.append(
            {
                "test": str(test),
                "status": "FAIL",
                "details": self._exc_info_to_string(err, test),
            }
        )

    def addError(self, test, err):
        super().addError(test, err)
        self.records.append(
            {
                "test": str(test),
                "status": "ERROR",
                "details": self._exc_info_to_string(err, test),
            }
        )

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        self.records.append(
            {
                "test": str(test),
                "status": "SKIP",
                "details": reason,
            }
        )


class JsonTestRunner(unittest.TextTestRunner):
    resultclass = JsonTestResult


def make_markdown_report(report: dict) -> str:
    lines: list[str] = []

    lines.append("# Nodo Python scenario diagnostics")
    lines.append("")
    lines.append(f"Generated at: `{report['generated_at']}`")
    lines.append(f"Repository root: `{report['repo_root']}`")
    lines.append(f"Scenario directory: `{report['scenario_dir']}`")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append(f"- Total: `{report['summary']['total']}`")
    lines.append(f"- Passed: `{report['summary']['passed']}`")
    lines.append(f"- Failed: `{report['summary']['failed']}`")
    lines.append(f"- Errors: `{report['summary']['errors']}`")
    lines.append(f"- Skipped: `{report['summary']['skipped']}`")
    lines.append("")
    lines.append("## Scenario results")
    lines.append("")

    for record in report["results"]:
        lines.append(f"### `{record['status']}` — `{record['test']}`")
        lines.append("")
        if record["details"]:
            lines.append("```text")
            lines.append(record["details"][-4000:])
            lines.append("```")
            lines.append("")

    return "\n".join(lines)


def main() -> int:
    repo_root = find_repo_root()
    diagnostics_python = repo_root / "diagnostics" / "python"
    scenario_dir = diagnostics_python / "scenarios"

    if str(diagnostics_python) not in sys.path:
        sys.path.insert(0, str(diagnostics_python))

    if not scenario_dir.is_dir():
        print(f"Scenario directory not found: {scenario_dir}")
        return 1

    report_dir = repo_root / "diagnostics" / "reports"
    report_dir.mkdir(parents=True, exist_ok=True)

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")

    loader = unittest.TestLoader()
    suite = loader.discover(
        start_dir=str(scenario_dir),
        pattern="test_*.py",
        top_level_dir=str(diagnostics_python),
    )

    runner = JsonTestRunner(verbosity=2)
    result = runner.run(suite)

    records = result.records

    summary = {
        "total": result.testsRun,
        "passed": sum(1 for item in records if item["status"] == "PASS"),
        "failed": len(result.failures),
        "errors": len(result.errors),
        "skipped": len(result.skipped),
    }

    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "repo_root": str(repo_root),
        "scenario_dir": str(scenario_dir),
        "summary": summary,
        "results": records,
    }

    json_path = report_dir / f"nodo_python_scenarios_{timestamp}.json"
    md_path = report_dir / f"nodo_python_scenarios_{timestamp}.md"

    json_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    md_path.write_text(
        make_markdown_report(report),
        encoding="utf-8",
    )

    print("")
    print("Python scenario report written:")
    print(f"- {md_path}")
    print(f"- {json_path}")

    if result.wasSuccessful():
        print("")
        print("All Python scenarios passed.")
        return 0

    print("")
    print("Some Python scenarios failed. Open the markdown report above.")
    return 1


if __name__ == "__main__":
    sys.exit(main())

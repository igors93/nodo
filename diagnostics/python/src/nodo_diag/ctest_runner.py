from __future__ import annotations

import os
import re
import shutil
import subprocess
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Iterable

DEFAULT_FOCUSED_TESTS = [
    "app_CommandLineLocalFlowTests",
    "app_CommandLinePersistentMempoolTests",
    "app_CommandLineRuntimeBlockTests",
    "node_FinalizedBlockStoreTests",
    "node_RuntimeStateLoaderTests",
]


@dataclass
class CommandResult:
    command: list[str]
    cwd: str
    returncode: int
    duration_seconds: float
    stdout: str
    stderr: str

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


def find_repo_root(start: Path | None = None) -> Path:
    current = (start or Path.cwd()).resolve()

    for candidate in [current, *current.parents]:
        if (candidate / "CMakeLists.txt").is_file() and (candidate / "src").is_dir():
            return candidate

    raise RuntimeError(
        "Could not find Nodo repository root. Run this script from inside the repository."
    )


def find_build_dirs(repo_root: Path) -> list[Path]:
    candidates: list[Path] = []

    for name in [
        "build",
        "cmake-build-debug",
        "cmake-build-release",
        "out/build",
        "Build",
    ]:
        path = repo_root / name
        if path.exists():
            candidates.append(path)

    for ctest_file in repo_root.rglob("CTestTestfile.cmake"):
        build_dir = ctest_file.parent
        if build_dir not in candidates:
            candidates.append(build_dir)

    return sorted(set(candidates))


def find_ctest_build_dir(repo_root: Path) -> Path:
    build_dirs = find_build_dirs(repo_root)

    for build_dir in build_dirs:
        if (build_dir / "CTestTestfile.cmake").is_file():
            return build_dir

    if build_dirs:
        return build_dirs[0]

    raise RuntimeError(
        "Could not find a CMake build directory. Run ./scripts/cmake_build.sh first."
    )


def read_failed_tests_from_ctest_log(build_dir: Path) -> list[str]:
    path = build_dir / "Testing" / "Temporary" / "LastTestsFailed.log"

    if not path.is_file():
        return []

    last_test_log = build_dir / "Testing" / "Temporary" / "LastTest.log"
    if last_test_log.is_file() and last_test_log.stat().st_mtime > path.stat().st_mtime:
        last_test_text = last_test_log.read_text(encoding="utf-8", errors="replace")
        if "***Failed" not in last_test_text:
            return []

    tests: list[str] = []

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.strip()
        if not stripped:
            continue

        if ":" in stripped:
            _, test_name = stripped.split(":", 1)
            test_name = test_name.strip()
        else:
            test_name = stripped

        if test_name and test_name not in tests:
            tests.append(test_name)

    return tests


def focused_ctests(build_dir: Path) -> list[str]:
    return read_failed_tests_from_ctest_log(build_dir) or list(DEFAULT_FOCUSED_TESTS)


def run_command(
    command: list[str],
    cwd: Path,
    timeout_seconds: int = 120,
) -> CommandResult:
    env = os.environ.copy()

    # This is harmless when unsupported, but useful if future C++ tests preserve temp dirs.
    env.setdefault("NODO_PRESERVE_TEST_ARTIFACTS", "1")

    started = time.monotonic()

    try:
        completed = subprocess.run(
            command,
            cwd=str(cwd),
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout_seconds,
            env=env,
        )
        return CommandResult(
            command=command,
            cwd=str(cwd),
            returncode=completed.returncode,
            duration_seconds=round(time.monotonic() - started, 3),
            stdout=completed.stdout,
            stderr=completed.stderr,
        )
    except subprocess.TimeoutExpired as error:
        stdout = error.stdout or ""
        stderr = error.stderr or ""
        if isinstance(stdout, bytes):
            stdout = stdout.decode(errors="replace")
        if isinstance(stderr, bytes):
            stderr = stderr.decode(errors="replace")

        return CommandResult(
            command=command,
            cwd=str(cwd),
            returncode=124,
            duration_seconds=round(time.monotonic() - started, 3),
            stdout=stdout,
            stderr=stderr + "\n[diagnostic] command timed out",
        )


def run_failed_ctests(build_dir: Path, tests: Iterable[str] | None = None) -> list[CommandResult]:
    ctest = shutil.which("ctest")
    if not ctest:
        raise RuntimeError("ctest was not found in PATH.")

    results: list[CommandResult] = []
    selected_tests = list(tests) if tests is not None else focused_ctests(build_dir)

    for test_name in selected_tests:
        results.append(
            run_command(
                [
                    ctest,
                    "-R",
                    f"^{re.escape(test_name)}$",
                    "--output-on-failure",
                    "-VV",
                ],
                cwd=build_dir,
                timeout_seconds=180,
            )
        )

    return results


def read_last_test_logs(build_dir: Path) -> dict[str, str]:
    logs: dict[str, str] = {}
    temporary = build_dir / "Testing" / "Temporary"

    if not temporary.exists():
        return logs

    for name in ["LastTest.log", "LastTestsFailed.log"]:
        path = temporary / name
        if path.is_file():
            logs[name] = path.read_text(encoding="utf-8", errors="replace")

    return logs


def classify_output(text: str) -> list[str]:
    patterns = {
        "empty_key_value_field": r"Empty value for key-value field",
        "unknown_key_value_field": (
            r"Unknown key-value field|Unexpected key-value field|not allowed"
        ),
        "finalized_block_persist_failure": r"Finalized block file should persist",
        "runtime_pipeline_not_finalized": (
            r"Runtime block pipeline.*not finalized|Pipeline should finalize"
        ),
        "reward_split_mismatch": r"reward.*does not match|fee split|validator fee allocation",
        "protection_reward_mismatch": (
            r"protection reward|ProtectionReward|protectionWork|protectionSummary"
        ),
        "governance_mismatch": r"governance|Governance",
        "monetary_firewall_mismatch": r"monetary firewall|MonetaryFirewall|supply ledger",
        "slashing_mismatch": r"slashing|CryptographicSlashing|sourcePenaltyDigest",
        "schema_id_mismatch": r"NODO_FINALIZED_BLOCK_V\\d+|Unsupported finalized block",
        "cli_flow_failure": r"CommandLine|command line|CLI",
    }

    found: list[str] = []

    for label, pattern in patterns.items():
        if re.search(pattern, text, re.IGNORECASE):
            found.append(label)

    return found


def extract_failure_windows(text: str, window: int = 8) -> list[str]:
    lines = text.splitlines()
    interesting = []

    markers = [
        "FAILED",
        "Failed",
        "Error",
        "error",
        "Exception",
        "should",
        "Invalid finalized block",
        "Empty value",
        "Unknown",
        "does not match",
    ]

    for index, line in enumerate(lines):
        if any(marker in line for marker in markers):
            start = max(index - window, 0)
            end = min(index + window + 1, len(lines))
            interesting.append("\n".join(lines[start:end]))

    # Keep report readable.
    return interesting[:20]

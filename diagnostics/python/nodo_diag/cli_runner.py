from __future__ import annotations

from dataclasses import dataclass, asdict
from pathlib import Path
import os
import subprocess
import time


@dataclass
class NodoCliResult:
    command: list[str]
    cwd: str
    returncode: int
    duration_seconds: float
    stdout: str
    stderr: str

    def output(self) -> str:
        return self.stdout + "\n" + self.stderr

    def succeeded(self) -> bool:
        return self.returncode == 0

    def failed(self) -> bool:
        return self.returncode != 0

    def to_dict(self) -> dict:
        return asdict(self)


def find_repo_root(start: Path | None = None) -> Path:
    current = (start or Path.cwd()).resolve()

    for candidate in [current, *current.parents]:
        if (candidate / "CMakeLists.txt").is_file() and (candidate / "src").is_dir():
            return candidate

    raise RuntimeError(
        "Could not find Nodo repository root. Run this from inside the Nodo repository."
    )


def find_nodo_binary(repo_root: Path | None = None) -> Path:
    root = repo_root or find_repo_root()

    env_bin = os.environ.get("NODO_BIN")
    if env_bin:
        candidate = Path(env_bin).expanduser().resolve()
        if candidate.is_file():
            return candidate
        raise RuntimeError(f"NODO_BIN points to a missing file: {candidate}")

    candidates = [
        root / "build" / "cmake" / "nodo",
        root / "build" / "cmake" / "nodo.exe",
        root / "build" / "nodo",
        root / "build" / "nodo.exe",
        root / "cmake-build-debug" / "nodo",
        root / "cmake-build-debug" / "nodo.exe",
        root / "cmake-build-release" / "nodo",
        root / "cmake-build-release" / "nodo.exe",
        root / "out" / "build" / "nodo",
        root / "out" / "build" / "nodo.exe",
    ]

    for candidate in candidates:
        if candidate.is_file():
            return candidate

    for candidate in list(root.rglob("nodo")) + list(root.rglob("nodo.exe")):
        if candidate.is_file() and candidate.name in {"nodo", "nodo.exe"}:
            return candidate

    raise RuntimeError(
        "Could not find Nodo binary. Build first with: "
        "NODO_BUILD_JOBS=1 CTEST_PARALLEL_LEVEL=1 ./scripts/cmake_test_all.sh"
    )


def run_nodo(
    args: list[str],
    repo_root: Path | None = None,
    timeout_seconds: int = 30,
    extra_env: dict[str, str] | None = None,
) -> NodoCliResult:
    root = repo_root or find_repo_root()
    nodo_bin = find_nodo_binary(root)

    command = [str(nodo_bin), *args]

    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)

    started = time.monotonic()

    try:
        completed = subprocess.run(
            command,
            cwd=str(root),
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout_seconds,
            env=env,
        )

        return NodoCliResult(
            command=command,
            cwd=str(root),
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

        return NodoCliResult(
            command=command,
            cwd=str(root),
            returncode=124,
            duration_seconds=round(time.monotonic() - started, 3),
            stdout=stdout,
            stderr=stderr + "\n[diagnostic] nodo command timed out",
        )


def assert_failed_with_text(
    result: NodoCliResult,
    expected_text: str,
) -> None:
    assert result.failed(), (
        "Expected command to fail, but it succeeded.\n"
        f"Command: {' '.join(result.command)}\n"
        f"stdout:\n{result.stdout}\n"
        f"stderr:\n{result.stderr}\n"
    )

    combined = result.output()

    assert expected_text in combined, (
        f"Expected output to contain: {expected_text!r}\n"
        f"Command: {' '.join(result.command)}\n"
        f"Return code: {result.returncode}\n"
        f"stdout:\n{result.stdout}\n"
        f"stderr:\n{result.stderr}\n"
    )
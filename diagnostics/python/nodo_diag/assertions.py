from __future__ import annotations

"""
Standalone assertion helpers for Nodo diagnostic scenarios.

These wrap NodoCliResult checks in clear, reusable functions.
Use the NodoBaseTest class methods when writing unittest.TestCase subclasses;
use these functions in scripts or one-off checks.
"""

from nodo_diag.cli_runner import NodoCliResult


def assert_succeeded(result: NodoCliResult, msg: str = "") -> None:
    assert result.succeeded(), (
        f"Expected command to succeed (rc=0) but got rc={result.returncode}.\n"
        f"{msg}\n"
        f"Command: {' '.join(result.command)}\n"
        f"stdout:\n{result.stdout}\n"
        f"stderr:\n{result.stderr}"
    )


def assert_failed(result: NodoCliResult, msg: str = "") -> None:
    assert result.failed(), (
        f"Expected command to fail (rc!=0) but it succeeded.\n"
        f"{msg}\n"
        f"Command: {' '.join(result.command)}\n"
        f"stdout:\n{result.stdout}\n"
        f"stderr:\n{result.stderr}"
    )


def assert_failed_with_text(result: NodoCliResult, text: str, msg: str = "") -> None:
    assert_failed(result, msg=msg)
    combined = result.output()
    assert text in combined, (
        f"Expected output to contain {text!r}.\n"
        f"{msg}\n"
        f"Command: {' '.join(result.command)}\n"
        f"Return code: {result.returncode}\n"
        f"stdout:\n{result.stdout}\n"
        f"stderr:\n{result.stderr}"
    )


def assert_succeeded_with_text(result: NodoCliResult, text: str, msg: str = "") -> None:
    assert_succeeded(result, msg=msg)
    combined = result.output()
    assert text in combined, (
        f"Expected output to contain {text!r}.\n"
        f"{msg}\n"
        f"Command: {' '.join(result.command)}\n"
        f"stdout:\n{result.stdout}\n"
        f"stderr:\n{result.stderr}"
    )


def assert_output_contains_all(result: NodoCliResult, texts: list[str], msg: str = "") -> None:
    combined = result.output()
    for text in texts:
        assert text in combined, (
            f"Expected output to contain {text!r} (checking all of {texts!r}).\n"
            f"{msg}\n"
            f"Command: {' '.join(result.command)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )


def assert_not_timed_out(result: NodoCliResult) -> None:
    assert result.returncode != 124, (
        "Command timed out (rc=124).\n"
        f"Command: {' '.join(result.command)}\n"
        f"stderr:\n{result.stderr}"
    )


def assert_no_crash(result: NodoCliResult) -> None:
    """Reject return codes that indicate a signal-based crash."""
    crash_codes = {134, 139, -11, -6}
    assert result.returncode not in crash_codes, (
        f"Command crashed with signal (rc={result.returncode}).\n"
        f"Command: {' '.join(result.command)}\n"
        f"stderr:\n{result.stderr}"
    )


def assert_clean_exit(result: NodoCliResult) -> None:
    """The command may succeed or fail gracefully — it must not crash or time out."""
    assert_not_timed_out(result)
    assert_no_crash(result)


def describe_result(result: NodoCliResult) -> str:
    """Return a human-readable summary of a NodoCliResult for debugging."""
    lines = [
        f"Command : {' '.join(result.command)}",
        f"Returned: {result.returncode} in {result.duration_seconds:.2f}s",
    ]
    if result.stdout.strip():
        lines.append(f"stdout  :\n{result.stdout.rstrip()}")
    if result.stderr.strip():
        lines.append(f"stderr  :\n{result.stderr.rstrip()}")
    return "\n".join(lines)

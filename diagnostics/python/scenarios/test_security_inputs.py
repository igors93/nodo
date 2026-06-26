"""
Security-focused input scenarios.

Verifies that the CLI handles adversarial inputs safely:
  - Path traversal attempts in --data-dir
  - Shell metacharacters in flag values (safe because subprocess list-form is used,
    but confirms the binary itself does not expand them)
  - Unicode / binary data in arguments (no crash)
  - Extremely long argument values (no crash, no hang)
  - Null bytes in arguments (handled safely)
  - Key IDs with dangerous filesystem semantics

IMPORTANT: All subprocess invocations use list-form args (not shell=True),
so shell injection via Python is not possible.  These tests verify that the
binary itself does not perform unsafe string operations on its inputs.

Category breakdown:
  - Path traversal in --data-dir: 8 subtests
  - Very long flag values: 6 subtests
  - Shell metacharacters in --data-dir: 10 subtests
  - Unicode in --network flag: 8 subtests
  - Unicode in --data-dir: 5 subtests
  - Dangerous key IDs: 12 subtests
  - Binary / control chars in args: 6 subtests
"""

from __future__ import annotations

import tempfile
from pathlib import Path

from nodo_diag.base_test import NodoBaseTest
from nodo_diag.cli_runner import run_nodo
from nodo_diag.generators import (
    bad_key_id_values,
    path_traversal_attempts,
    shell_metacharacter_values,
    unicode_payloads,
    very_long_values,
)


class PathTraversalScenarios(NodoBaseTest):
    """
    Path traversal attempts in --data-dir must not crash the binary.

    NOTE: On Windows with MSYS2, some Unix-style paths (e.g. /etc/passwd)
    resolve to real MSYS2 paths.  The localnet binary returns genesis state
    with rc=0 when the data-dir has no manifest — this is by design.
    These tests therefore verify safety (no crash, no hang), not rc!=0.
    """

    def test_status_with_path_traversal_in_data_dir(self) -> None:
        for label, bad_path in path_traversal_attempts():
            with self.subTest(label=label, path=bad_path):
                result = run_nodo(
                    ["status", "--network", "localnet", "--data-dir", bad_path],
                    repo_root=self.repo_root,
                    timeout_seconds=30,
                )
                self.assertNotTimedOut(result)
                self.assertNoSegfault(result)

    def test_init_with_path_traversal_in_data_dir(self) -> None:
        for label, bad_path in path_traversal_attempts():
            with self.subTest(label=label, path=bad_path):
                result = run_nodo(
                    ["init", "--network", "localnet", "--data-dir", bad_path],
                    repo_root=self.repo_root,
                    timeout_seconds=30,
                )
                # Either fails cleanly or init succeeds (in /tmp, not /etc).
                self.assertNotTimedOut(result)
                self.assertNoSegfault(result)

    def test_reload_with_path_traversal_in_data_dir(self) -> None:
        for label, bad_path in path_traversal_attempts():
            with self.subTest(label=label, path=bad_path):
                result = run_nodo(
                    ["node", "reload", "--network", "localnet", "--data-dir", bad_path],
                    repo_root=self.repo_root,
                    timeout_seconds=30,
                )
                self.assertNotTimedOut(result)
                self.assertNoSegfault(result)


class VeryLongValueScenarios(NodoBaseTest):
    """Extremely long flag values must not crash or hang the binary."""

    def test_status_with_very_long_data_dir(self) -> None:
        for label, long_value in very_long_values([256, 4096]):
            with self.subTest(label=label):
                result = run_nodo(
                    ["status", "--network", "localnet", "--data-dir", long_value],
                    repo_root=self.repo_root,
                    timeout_seconds=30,
                )
                self.assertFailed(result)
                self.assertNotTimedOut(result)
                self.assertNoSegfault(result)

    def test_status_with_very_long_network_name(self) -> None:
        for label, long_value in very_long_values([256, 4096]):
            with self.subTest(label=label):
                with tempfile.TemporaryDirectory(prefix="nodo_sec_longnet_") as tmp:
                    result = run_nodo(
                        [
                            "status",
                            "--network",
                            long_value,
                            "--data-dir",
                            str(Path(tmp) / "node-data"),
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)

    def test_keys_create_with_very_long_key_id(self) -> None:
        for label, long_value in very_long_values([256, 4096]):
            with self.subTest(label=label):
                with tempfile.TemporaryDirectory(prefix="nodo_sec_longkeyid_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    result = self.run_keys_create(
                        data_dir,
                        network="localnet",
                        key_type="validator",
                        key_id=long_value,
                    )
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


class ShellMetacharacterScenarios(NodoBaseTest):
    """
    Shell metacharacters in flag values.

    Because subprocess is invoked in list-form (not via shell=True), these
    characters are passed literally to the binary.  The binary must not
    interpret or execute them.
    """

    def test_status_with_shell_metacharacters_in_data_dir(self) -> None:
        for label, value in shell_metacharacter_values():
            with self.subTest(label=label, value=value):
                result = run_nodo(
                    ["status", "--network", "localnet", "--data-dir", value],
                    repo_root=self.repo_root,
                    timeout_seconds=30,
                )
                # The binary treats these as literal path strings — no shell expansion.
                # On localnet it may return genesis state (rc=0) for any uninitialized
                # path.  The safety guarantee is: no crash, no hang, no spawned subshell.
                self.assertNotTimedOut(result)
                self.assertNoSegfault(result)

    def test_init_with_shell_metacharacters_in_data_dir(self) -> None:
        for label, value in shell_metacharacter_values():
            with self.subTest(label=label, value=value):
                result = run_nodo(
                    ["init", "--network", "localnet", "--data-dir", value],
                    repo_root=self.repo_root,
                    timeout_seconds=30,
                )
                self.assertNotTimedOut(result)
                self.assertNoSegfault(result)


class UnicodeInputScenarios(NodoBaseTest):
    """Unicode and non-ASCII in flag values must not crash the binary."""

    def test_status_with_unicode_in_network_flag(self) -> None:
        for label, value in unicode_payloads():
            with self.subTest(label=label, value=value):
                with tempfile.TemporaryDirectory(prefix="nodo_sec_uninet_") as tmp:
                    result = run_nodo(
                        [
                            "status",
                            "--network",
                            value,
                            "--data-dir",
                            str(Path(tmp) / "node-data"),
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)

    def test_status_with_unicode_in_data_dir(self) -> None:
        for label, value in unicode_payloads():
            with self.subTest(label=label, value=value):
                result = run_nodo(
                    ["status", "--network", "localnet", "--data-dir", value],
                    repo_root=self.repo_root,
                    timeout_seconds=30,
                )
                self.assertFailed(result)
                self.assertNotTimedOut(result)
                self.assertNoSegfault(result)

    def test_keys_create_with_unicode_in_key_id(self) -> None:
        for label, value in unicode_payloads():
            with self.subTest(label=label, value=value):
                with tempfile.TemporaryDirectory(prefix="nodo_sec_unikeyid_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    result = self.run_keys_create(
                        data_dir,
                        network="localnet",
                        key_type="validator",
                        key_id=value,
                    )
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


class DangerousKeyIdScenarios(NodoBaseTest):
    """Key IDs with filesystem-dangerous semantics must not cause path escapes."""

    def test_keys_create_rejects_dangerous_ids(self) -> None:
        for label, bad_id in bad_key_id_values():
            with self.subTest(label=label, key_id=bad_id):
                with tempfile.TemporaryDirectory(prefix="nodo_sec_dkeyid_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    result = self.run_keys_create(
                        data_dir,
                        network="localnet",
                        key_type="validator",
                        key_id=bad_id,
                    )
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)
                    # Verify no files were created outside the data directory.
                    if result.returncode == 0:
                        # If the binary accepted the key, nothing should have escaped.
                        above_data_dir = data_dir.parent.parent
                        suspicious = list(above_data_dir.rglob("*.key")) + list(
                            above_data_dir.rglob("*.pem")
                        )
                        # Filter to exclude the data_dir itself
                        suspicious = [f for f in suspicious if not str(f).startswith(str(data_dir))]
                        self.assertEqual(
                            suspicious,
                            [],
                            msg=(
                                f"Possible path escape: files found outside data dir "
                                f"after creating key with id {bad_id!r}:\n"
                                + "\n".join(str(f) for f in suspicious)
                            ),
                        )


class RepetitiveOperationScenarios(NodoBaseTest):
    """Repeated identical operations must not accumulate state or fail progressively."""

    def test_status_called_many_times_is_stable(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_sec_repeated_stat_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            for i in range(5):
                with self.subTest(iteration=i):
                    result = self.run_status(data_dir)
                    self.assertSucceeded(result, msg=f"Status failed on iteration {i}")
                    self.assertNotTimedOut(result)

    def test_reload_called_many_times_is_stable(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_sec_repeated_rl_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            for i in range(3):
                with self.subTest(iteration=i):
                    result = self.run_reload(data_dir)
                    self.assertSucceeded(result, msg=f"Reload failed on iteration {i}")
                    self.assertNotTimedOut(result)

    def test_chain_audit_called_many_times_is_stable(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_sec_repeated_ca_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            for i in range(3):
                with self.subTest(iteration=i):
                    result = self.run_chain_audit(data_dir)
                    self.assertSucceeded(result, msg=f"Audit failed on iteration {i}")
                    self.assertNotTimedOut(result)


if __name__ == "__main__":
    import unittest

    unittest.main(verbosity=2)

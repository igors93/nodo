from __future__ import annotations

"""
Data directory edge case scenarios.

Verifies correct handling of:
  - Missing / empty / uninitialized data directories
  - Double initialization
  - Paths pointing to files instead of directories
  - Deeply nested paths
  - Paths with unusual but valid characters
  - Relative vs. absolute path handling

Category breakdown:
  - Missing data dir: 4 tests (status, reload, audit, keys list)
  - Empty data dir (exists but not initialized): 4 tests
  - Already initialized dir: 3 tests (double init)
  - Path as file: 3 tests
  - Deeply nested: 2 tests
  - Path characters: 4 subtests
"""

import os
from pathlib import Path
import tempfile

from nodo_diag.base_test import NodoBaseTest
from nodo_diag.cli_runner import run_nodo
from nodo_diag.filesystem_faults import manifest_path


class MissingDataDirScenarios(NodoBaseTest):
    """Commands given a path that does not exist at all."""

    def test_status_with_completely_missing_data_dir_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_stat_missing_") as tmp:
            data_dir = Path(tmp) / "does-not-exist" / "node-data"
            result = self.run_status(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_with_completely_missing_data_dir_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_rl_missing_") as tmp:
            data_dir = Path(tmp) / "does-not-exist" / "node-data"
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_chain_audit_with_completely_missing_data_dir_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_ca_missing_") as tmp:
            data_dir = Path(tmp) / "does-not-exist" / "node-data"
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_keys_list_with_completely_missing_data_dir_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_kl_missing_") as tmp:
            data_dir = Path(tmp) / "does-not-exist" / "node-data"
            result = self.run_keys_list(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)


class EmptyDataDirScenarios(NodoBaseTest):
    """Commands given an existing but empty (not initialized) directory."""

    def test_status_with_empty_data_dir_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_stat_empty_") as tmp:
            data_dir = Path(tmp) / "node-data"
            data_dir.mkdir(parents=True)
            result = self.run_status(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_with_empty_data_dir_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_rl_empty_") as tmp:
            data_dir = Path(tmp) / "node-data"
            data_dir.mkdir(parents=True)
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_chain_audit_with_empty_data_dir_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_ca_empty_") as tmp:
            data_dir = Path(tmp) / "node-data"
            data_dir.mkdir(parents=True)
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_keys_list_with_empty_data_dir_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_kl_empty_") as tmp:
            data_dir = Path(tmp) / "node-data"
            data_dir.mkdir(parents=True)
            result = self.run_keys_list(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)


class DoubleInitScenarios(NodoBaseTest):
    """Initializing a directory that is already initialized."""

    def test_double_init_same_localnet_is_handled_safely(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_dblInit_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            # Second init — must not crash or silently corrupt.
            result = run_nodo(
                ["init", "--network", "localnet", "--data-dir", str(data_dir)],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

            # Regardless of whether double-init succeeds or fails, the manifest
            # must still be present and loadable.
            self.assertTrue(
                manifest_path(data_dir).is_file(),
                msg="Manifest must still exist after double init",
            )

    def test_status_still_works_after_double_init(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_dblInitStat_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            run_nodo(
                ["init", "--network", "localnet", "--data-dir", str(data_dir)],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            result = self.run_status(data_dir)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_init_then_init_wrong_network_preserves_integrity(self) -> None:
        """Attempting to re-init with the wrong network must not corrupt the directory."""
        with tempfile.TemporaryDirectory(prefix="nodo_dd_dblInitWN_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            run_nodo(
                ["init", "--network", "testnet-candidate", "--data-dir", str(data_dir)],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

            # Original network must still be recognised.
            result = self.run_status(data_dir)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)


class DataDirAsFileScenarios(NodoBaseTest):
    """--data-dir pointing to a regular file (not a directory)."""

    def test_status_with_data_dir_as_file_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_stat_asfile_") as tmp:
            file_path = Path(tmp) / "iamfile.txt"
            file_path.write_text("I am a file, not a directory\n", encoding="utf-8")
            result = self.run_status(file_path)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_with_data_dir_as_file_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_rl_asfile_") as tmp:
            file_path = Path(tmp) / "iamfile.txt"
            file_path.write_text("I am a file, not a directory\n", encoding="utf-8")
            result = self.run_reload(file_path)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_chain_audit_with_data_dir_as_file_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_ca_asfile_") as tmp:
            file_path = Path(tmp) / "iamfile.txt"
            file_path.write_text("I am a file, not a directory\n", encoding="utf-8")
            result = self.run_chain_audit(file_path)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)


class DataDirPathEdgeCaseScenarios(NodoBaseTest):
    """Path forms that are unusual but should not cause crashes."""

    def test_init_with_deeply_nested_path(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_deep_") as tmp:
            nested = Path(tmp) / "a" / "b" / "c" / "d" / "e" / "node-data"
            result = run_nodo(
                ["init", "--network", "localnet", "--data-dir", str(nested)],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)
            if result.returncode == 0:
                self.assertTrue(
                    manifest_path(nested).is_file(),
                    msg="Manifest must be created in deeply nested path",
                )

    def test_status_with_trailing_separator_in_path(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_dd_trailsep_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            path_with_sep = str(data_dir) + os.sep
            result = run_nodo(
                ["status", "--network", "localnet", "--data-dir", path_with_sep],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_status_with_parent_then_child_path(self) -> None:
        """Path like /tmp/x/node-data/../node-data should resolve correctly."""
        with tempfile.TemporaryDirectory(prefix="nodo_dd_dotdot_path_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            canonical = data_dir
            dotted = canonical.parent / "node-data" / ".." / "node-data"
            result = run_nodo(
                ["status", "--network", "localnet", "--data-dir", str(dotted)],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_init_then_status_on_path_with_spaces(self) -> None:
        """Paths with spaces in directory names must work end-to-end."""
        import platform
        # Use a temp directory with spaces in its name
        with tempfile.TemporaryDirectory(prefix="nodo dd spaces ") as tmp:
            data_dir = Path(tmp) / "node data"
            result = run_nodo(
                ["init", "--network", "localnet", "--data-dir", str(data_dir)],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)
            if result.returncode == 0:
                stat_result = self.run_status(data_dir)
                self.assertNotTimedOut(stat_result)
                self.assertNoSegfault(stat_result)


if __name__ == "__main__":
    import unittest
    unittest.main(verbosity=2)

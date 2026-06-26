from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from nodo_diag.cli_runner import (
    assert_failed_with_text,
    find_nodo_binary,
    find_repo_root,
    run_nodo,
)
from nodo_diag.filesystem_faults import (
    manifest_path,
    replace_key_value,
)


class RuntimeReloadAuditScenarios(unittest.TestCase):
    def setUp(self) -> None:
        self.repo_root = find_repo_root()

        try:
            find_nodo_binary(self.repo_root)
        except RuntimeError as error:
            self.skipTest(str(error))

    def initialize_localnet(self, temp_root: Path) -> Path:
        data_dir = temp_root / "node-data"

        result = run_nodo(
            [
                "init",
                "--network",
                "localnet",
                "--data-dir",
                str(data_dir),
            ],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )

        self.assertEqual(
            result.returncode,
            0,
            msg=(
                "Nodo init should succeed before runtime reload/audit scenarios.\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}\n"
            ),
        )

        self.assertTrue(
            manifest_path(data_dir).is_file(),
            msg=f"Manifest was not created: {manifest_path(data_dir)}",
        )

        return data_dir

    def run_reload(self, data_dir: Path):
        return run_nodo(
            [
                "node",
                "reload",
                "--network",
                "localnet",
                "--data-dir",
                str(data_dir),
            ],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )

    def run_chain_audit(self, data_dir: Path):
        return run_nodo(
            [
                "chain",
                "audit",
                "--network",
                "localnet",
                "--data-dir",
                str(data_dir),
            ],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )

    def test_reload_succeeds_before_corruption(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reload_clean_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            result = self.run_reload(data_dir)

            self.assertEqual(
                result.returncode,
                0,
                msg=(
                    "Clean localnet runtime should reload successfully.\n"
                    f"stdout:\n{result.stdout}\n"
                    f"stderr:\n{result.stderr}\n"
                ),
            )

            self.assertIn(
                "Nodo runtime reloaded.",
                result.output(),
                msg=(
                    "Reload should print success message.\n"
                    f"stdout:\n{result.stdout}\n"
                    f"stderr:\n{result.stderr}\n"
                ),
            )

    def test_chain_audit_succeeds_before_corruption(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_audit_clean_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            result = self.run_chain_audit(data_dir)

            self.assertEqual(
                result.returncode,
                0,
                msg=(
                    "Clean localnet chain audit should succeed.\n"
                    f"stdout:\n{result.stdout}\n"
                    f"stderr:\n{result.stderr}\n"
                ),
            )

    def test_reload_rejects_wrong_genesis_id(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reload_bad_genesis_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            replace_key_value(
                manifest_path(data_dir),
                "genesisConfigId",
                "bad-python-diagnostic-genesis-id",
            )

            result = self.run_reload(data_dir)

            assert_failed_with_text(result, "Failed to reload Nodo runtime")

    def test_chain_audit_rejects_wrong_genesis_id(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_audit_bad_genesis_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            replace_key_value(
                manifest_path(data_dir),
                "genesisConfigId",
                "bad-python-diagnostic-genesis-id",
            )

            result = self.run_chain_audit(data_dir)

            assert_failed_with_text(result, "chain audit")

    def test_reload_rejects_wrong_latest_hash(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reload_bad_hash_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            replace_key_value(
                manifest_path(data_dir),
                "latestBlockHash",
                "bad-python-diagnostic-block-hash",
            )

            result = self.run_reload(data_dir)

            assert_failed_with_text(result, "Failed to reload Nodo runtime")

    def test_chain_audit_rejects_wrong_latest_hash(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_audit_bad_hash_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            replace_key_value(
                manifest_path(data_dir),
                "latestBlockHash",
                "bad-python-diagnostic-block-hash",
            )

            result = self.run_chain_audit(data_dir)

            assert_failed_with_text(result, "chain audit")

    def test_reload_rejects_wrong_latest_state_root(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reload_bad_state_root_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            replace_key_value(
                manifest_path(data_dir),
                "latestStateRoot",
                "bad-python-diagnostic-state-root",
            )

            result = self.run_reload(data_dir)

            assert_failed_with_text(result, "Failed to reload Nodo runtime")

    def test_chain_audit_rejects_wrong_latest_state_root(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_audit_bad_state_root_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            replace_key_value(
                manifest_path(data_dir),
                "latestStateRoot",
                "bad-python-diagnostic-state-root",
            )

            result = self.run_chain_audit(data_dir)

            assert_failed_with_text(result, "chain audit")

    def test_reload_rejects_missing_finalized_block_declared_in_manifest(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reload_missing_block_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            replace_key_value(
                manifest_path(data_dir),
                "latestBlockHeight",
                "1",
            )

            result = self.run_reload(data_dir)

            assert_failed_with_text(result, "Failed to reload Nodo runtime")

    def test_chain_audit_rejects_missing_finalized_block_declared_in_manifest(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_audit_missing_block_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            replace_key_value(
                manifest_path(data_dir),
                "latestBlockHeight",
                "1",
            )

            result = self.run_chain_audit(data_dir)

            assert_failed_with_text(result, "chain audit")


if __name__ == "__main__":
    unittest.main(verbosity=2)

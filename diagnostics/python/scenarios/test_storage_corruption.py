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
    append_key_value,
    break_file_canonical_order,
    delete_file,
    manifest_path,
    remove_key_value_line,
    replace_key_value,
)


class StorageCorruptionScenarios(unittest.TestCase):
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
                "Nodo init should succeed before corruption.\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}\n"
            ),
        )

        self.assertTrue(
            manifest_path(data_dir).is_file(),
            msg=f"Manifest was not created: {manifest_path(data_dir)}",
        )

        return data_dir

    def run_status(self, data_dir: Path):
        return run_nodo(
            [
                "status",
                "--network",
                "localnet",
                "--data-dir",
                str(data_dir),
            ],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )

    def test_status_rejects_missing_manifest(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_storage_missing_manifest_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            delete_file(manifest_path(data_dir))

            result = self.run_status(data_dir)

            assert_failed_with_text(result, "Failed to read Nodo status")

    def test_status_rejects_manifest_with_unknown_field(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_storage_unknown_field_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            append_key_value(
                manifest_path(data_dir),
                "unexpectedPythonDiagnosticField",
                "bad",
            )

            result = self.run_status(data_dir)

            assert_failed_with_text(result, "Failed to read Nodo status")

    def test_status_rejects_manifest_with_missing_required_field(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_storage_missing_field_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            remove_key_value_line(
                manifest_path(data_dir),
                "chainId",
            )

            result = self.run_status(data_dir)

            assert_failed_with_text(result, "Failed to read Nodo status")

    def test_status_rejects_manifest_with_malformed_number(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_storage_bad_number_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            replace_key_value(
                manifest_path(data_dir),
                "latestBlockHeight",
                "not-a-number",
            )

            result = self.run_status(data_dir)

            assert_failed_with_text(result, "Failed to read Nodo status")

    def test_status_rejects_manifest_with_zero_validators(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_storage_zero_validators_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            replace_key_value(
                manifest_path(data_dir),
                "validatorCount",
                "0",
            )

            result = self.run_status(data_dir)

            assert_failed_with_text(result, "Failed to read Nodo status")

    def test_status_rejects_manifest_that_is_not_canonical(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_storage_not_canonical_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            break_file_canonical_order(
                manifest_path(data_dir),
            )

            result = self.run_status(data_dir)

            assert_failed_with_text(result, "Failed to read Nodo status")

    def test_status_rejects_wrong_selected_network(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_storage_wrong_network_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            result = run_nodo(
                [
                    "status",
                    "--network",
                    "testnet-candidate",
                    "--data-dir",
                    str(data_dir),
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

            assert_failed_with_text(result, "Data directory belongs to network")


if __name__ == "__main__":
    unittest.main(verbosity=2)

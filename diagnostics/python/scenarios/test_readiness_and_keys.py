from __future__ import annotations

from pathlib import Path
import tempfile
import unittest

from nodo_diag.cli_runner import (
    assert_failed_with_text,
    find_nodo_binary,
    find_repo_root,
    run_nodo,
)


class ReadinessAndKeyScenarios(unittest.TestCase):
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
                "Nodo init should succeed before readiness/key scenarios.\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}\n"
            ),
        )

        return data_dir

    def create_localnet_validator_key(self, data_dir: Path) -> None:
        result = run_nodo(
            [
                "keys",
                "create",
                "--network",
                "localnet",
                "--data-dir",
                str(data_dir),
                "--type",
                "validator",
                "--key-id",
                "local-validator",
            ],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )

        self.assertEqual(
            result.returncode,
            0,
            msg=(
                "Creating localnet validator key should succeed.\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}\n"
            ),
        )

        self.assertIn(
            "Nodo development key created.",
            result.output(),
            msg=(
                "Expected development key warning/output.\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}\n"
            ),
        )

    def test_keys_create_before_init_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_keys_before_init_") as temp:
            data_dir = Path(temp) / "node-data"

            result = run_nodo(
                [
                    "keys",
                    "create",
                    "--network",
                    "localnet",
                    "--data-dir",
                    str(data_dir),
                    "--type",
                    "validator",
                    "--key-id",
                    "local-validator",
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

            assert_failed_with_text(result, "Cannot create key before init")

    def test_keys_list_before_init_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_keys_list_before_init_") as temp:
            data_dir = Path(temp) / "node-data"

            result = run_nodo(
                [
                    "keys",
                    "list",
                    "--data-dir",
                    str(data_dir),
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

            assert_failed_with_text(result, "Cannot list keys before init")

    def test_keys_create_rejects_network_mismatch(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_keys_network_mismatch_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            result = run_nodo(
                [
                    "keys",
                    "create",
                    "--network",
                    "testnet-candidate",
                    "--data-dir",
                    str(data_dir),
                    "--type",
                    "validator",
                    "--key-id",
                    "local-validator",
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

            assert_failed_with_text(result, "Data directory belongs to network")

    def test_testnet_readiness_without_key_id_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_readiness_no_key_id_") as temp:
            data_dir = Path(temp) / "node-data"

            result = run_nodo(
                [
                    "testnet",
                    "readiness",
                    "--network",
                    "testnet-candidate",
                    "--data-dir",
                    str(data_dir),
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

            assert_failed_with_text(result, "Official network readiness requires --key-id")

    def test_diagnostics_without_key_id_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_diagnostics_no_key_id_") as temp:
            data_dir = Path(temp) / "node-data"

            result = run_nodo(
                [
                    "diagnostics",
                    "--network",
                    "testnet-candidate",
                    "--data-dir",
                    str(data_dir),
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

            assert_failed_with_text(result, "Official network diagnostics requires --key-id")

    def test_testnet_readiness_rejects_localnet_data_directory(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_readiness_wrong_network_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            result = run_nodo(
                [
                    "testnet",
                    "readiness",
                    "--network",
                    "testnet-candidate",
                    "--data-dir",
                    str(data_dir),
                    "--key-id",
                    "local-validator",
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

            assert_failed_with_text(result, "Data directory belongs to network")

    def test_diagnostics_rejects_localnet_data_directory(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_diagnostics_wrong_network_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            result = run_nodo(
                [
                    "diagnostics",
                    "--network",
                    "testnet-candidate",
                    "--data-dir",
                    str(data_dir),
                    "--key-id",
                    "local-validator",
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

            assert_failed_with_text(result, "Data directory belongs to network")

    def test_localnet_key_creation_prints_development_warning(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_warning_") as temp:
            data_dir = self.initialize_localnet(Path(temp))

            result = run_nodo(
                [
                    "keys",
                    "create",
                    "--network",
                    "localnet",
                    "--data-dir",
                    str(data_dir),
                    "--type",
                    "validator",
                    "--key-id",
                    "local-validator",
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

            self.assertEqual(
                result.returncode,
                0,
                msg=(
                    "Localnet key creation should succeed.\n"
                    f"stdout:\n{result.stdout}\n"
                    f"stderr:\n{result.stderr}\n"
                ),
            )

            self.assertIn(
                "deterministic localnet-only development key",
                result.output(),
                msg=(
                    "Key creation should warn that the key is localnet-only development material.\n"
                    f"stdout:\n{result.stdout}\n"
                    f"stderr:\n{result.stderr}\n"
                ),
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
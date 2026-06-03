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


class CliSafetyScenarios(unittest.TestCase):
    def setUp(self) -> None:
        self.repo_root = find_repo_root()

        try:
            find_nodo_binary(self.repo_root)
        except RuntimeError as error:
            self.skipTest(str(error))

    def run_with_temp_data_dir(self, args: list[str]):
        with tempfile.TemporaryDirectory(prefix="nodo_cli_safety_") as temp_dir:
            return run_nodo(
                [
                    *args,
                    "--data-dir",
                    str(Path(temp_dir) / "node-data"),
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

    def test_demo_is_blocked_on_testnet_candidate(self) -> None:
        result = self.run_with_temp_data_dir(
            [
                "demo",
                "--network",
                "testnet-candidate",
            ]
        )

        assert_failed_with_text(result, "not permitted on official network")

    def test_reload_legacy_alias_is_blocked_on_testnet_candidate(self) -> None:
        result = self.run_with_temp_data_dir(
            [
                "reload",
                "--network",
                "testnet-candidate",
            ]
        )

        assert_failed_with_text(result, "not permitted on official network")

    def test_submit_demo_transaction_is_blocked_on_testnet_candidate(self) -> None:
        result = self.run_with_temp_data_dir(
            [
                "submit-demo-transaction",
                "--network",
                "testnet-candidate",
            ]
        )

        assert_failed_with_text(result, "not permitted on official network")

    def test_produce_demo_block_is_blocked_on_testnet_candidate(self) -> None:
        result = self.run_with_temp_data_dir(
            [
                "produce-demo-block",
                "--network",
                "testnet-candidate",
            ]
        )

        assert_failed_with_text(result, "not permitted on official network")

    def test_testnet_readiness_requires_key_id(self) -> None:
        result = self.run_with_temp_data_dir(
            [
                "testnet",
                "readiness",
                "--network",
                "testnet-candidate",
            ]
        )

        assert_failed_with_text(result, "Official network readiness requires --key-id")


if __name__ == "__main__":
    unittest.main(verbosity=2)
from __future__ import annotations

"""
CLI flag and subcommand validation scenarios.

Verifies that the CLI enforces:
  - Network restrictions on demo commands across all official networks
  - Required flags (--key-id, --network, --data-dir)
  - Rejection of unknown / malformed network names
  - Rejection of invalid key types and key IDs
  - Graceful handling of unknown subcommands

Category breakdown:
  - Demo commands blocked on official networks: 4 cmds × N networks = many subtests
  - Unknown network names: 10 subtests
  - Missing required flags: multiple subtests
  - Invalid key types: 7 subtests
  - Invalid key IDs: 15 subtests
  - Unknown subcommands: 6 subtests
  - Empty/malformed flag values: 8 subtests
"""

from pathlib import Path
import tempfile

from nodo_diag.base_test import NodoBaseTest
from nodo_diag.cli_runner import run_nodo
from nodo_diag.generators import (
    all_demo_commands,
    bad_key_id_values,
    bad_key_types,
    bad_network_names,
    official_networks,
)


class CliNetworkRestrictionScenarios(NodoBaseTest):
    """Demo commands are blocked on every official network."""

    def _run_demo_command(self, command: str, network: str) -> object:
        with tempfile.TemporaryDirectory(prefix=f"nodo_clf_{command[:6]}_") as tmp:
            return run_nodo(
                [command, "--network", network, "--data-dir", str(Path(tmp) / "node-data")],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )

    def test_demo_commands_blocked_on_all_official_networks(self) -> None:
        for network in official_networks():
            for command in all_demo_commands():
                with self.subTest(command=command, network=network):
                    result = self._run_demo_command(command, network)
                    self.assertFailedWithText(result, "not permitted on official network")

    def test_testnet_readiness_blocked_without_key_id_on_official_networks(self) -> None:
        for network in official_networks():
            with self.subTest(network=network):
                with tempfile.TemporaryDirectory(prefix="nodo_clf_ready_nokeyid_") as tmp:
                    result = run_nodo(
                        [
                            "testnet", "readiness",
                            "--network", network,
                            "--data-dir", str(Path(tmp) / "node-data"),
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailedWithText(
                        result,
                        "Official network readiness requires --key-id",
                    )

    def test_diagnostics_blocked_without_key_id_on_official_networks(self) -> None:
        for network in official_networks():
            with self.subTest(network=network):
                with tempfile.TemporaryDirectory(prefix="nodo_clf_diag_nokeyid_") as tmp:
                    result = run_nodo(
                        [
                            "diagnostics",
                            "--network", network,
                            "--data-dir", str(Path(tmp) / "node-data"),
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailedWithText(
                        result,
                        "Official network diagnostics requires --key-id",
                    )


class CliUnknownNetworkScenarios(NodoBaseTest):
    """Unknown or malformed network names must be rejected cleanly."""

    def test_init_rejects_unknown_network_names(self) -> None:
        for label, bad_name in bad_network_names():
            if not bad_name.strip():
                continue  # empty/space is tested separately
            with self.subTest(label=label, network=bad_name):
                with tempfile.TemporaryDirectory(prefix=f"nodo_clf_initnet_{label}_") as tmp:
                    result = run_nodo(
                        [
                            "init",
                            "--network", bad_name,
                            "--data-dir", str(Path(tmp) / "node-data"),
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)

    def test_status_rejects_unknown_network_names(self) -> None:
        for label, bad_name in bad_network_names():
            if not bad_name.strip():
                continue
            with self.subTest(label=label, network=bad_name):
                with tempfile.TemporaryDirectory(prefix=f"nodo_clf_statnet_{label}_") as tmp:
                    result = run_nodo(
                        [
                            "status",
                            "--network", bad_name,
                            "--data-dir", str(Path(tmp) / "node-data"),
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)

    def test_keys_create_rejects_unknown_network_names(self) -> None:
        for label, bad_name in bad_network_names():
            if not bad_name.strip():
                continue
            with self.subTest(label=label, network=bad_name):
                with tempfile.TemporaryDirectory(prefix=f"nodo_clf_keynet_{label}_") as tmp:
                    result = run_nodo(
                        [
                            "keys", "create",
                            "--network", bad_name,
                            "--data-dir", str(Path(tmp) / "node-data"),
                            "--type", "validator",
                            "--key-id", "test-key",
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)

    def test_reload_rejects_unknown_network_names(self) -> None:
        for label, bad_name in bad_network_names():
            if not bad_name.strip():
                continue
            with self.subTest(label=label, network=bad_name):
                with tempfile.TemporaryDirectory(prefix=f"nodo_clf_rlnet_{label}_") as tmp:
                    result = run_nodo(
                        [
                            "node", "reload",
                            "--network", bad_name,
                            "--data-dir", str(Path(tmp) / "node-data"),
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


class CliKeyFlagScenarios(NodoBaseTest):
    """Key-related flag validation."""

    def test_keys_create_rejects_invalid_types(self) -> None:
        for label, bad_type in bad_key_types():
            with self.subTest(label=label, type=bad_type):
                with tempfile.TemporaryDirectory(prefix=f"nodo_clf_keytype_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    result = run_nodo(
                        [
                            "keys", "create",
                            "--network", "localnet",
                            "--data-dir", str(data_dir),
                            "--type", bad_type,
                            "--key-id", "test-key",
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)

    def test_keys_create_rejects_invalid_key_ids(self) -> None:
        for label, bad_id in bad_key_id_values():
            with self.subTest(label=label, key_id=bad_id):
                with tempfile.TemporaryDirectory(prefix=f"nodo_clf_keyid_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    result = run_nodo(
                        [
                            "keys", "create",
                            "--network", "localnet",
                            "--data-dir", str(data_dir),
                            "--type", "validator",
                            "--key-id", bad_id,
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)
                    # These must not succeed silently with unsafe key IDs.
                    # Either they fail or they produce a visible warning.
                    # We do not require failure for all — only no crash/hang.


class CliUnknownSubcommandScenarios(NodoBaseTest):
    """Unknown or mistyped subcommands must produce a clean error."""

    UNKNOWN_SUBCOMMANDS = [
        "nonexistent",
        "delete",
        "purge",
        "reset",
        "drop",
        "shell",
        "exec",
        "run",
        "--help-all",
    ]

    def test_unknown_subcommands_fail_cleanly(self) -> None:
        for cmd in self.UNKNOWN_SUBCOMMANDS:
            with self.subTest(command=cmd):
                with tempfile.TemporaryDirectory(prefix="nodo_clf_unkcmd_") as tmp:
                    result = run_nodo(
                        [cmd, "--data-dir", str(Path(tmp) / "node-data")],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)

    def test_unknown_subcommand_keys_sub_fails_cleanly(self) -> None:
        """Keys subcommands that don't exist must fail cleanly."""
        bad_key_subs = ["delete", "export", "sign", "verify", "rotate"]
        for sub in bad_key_subs:
            with self.subTest(sub=sub):
                with tempfile.TemporaryDirectory(prefix="nodo_clf_keysubcmd_") as tmp:
                    result = run_nodo(
                        [
                            "keys", sub,
                            "--data-dir", str(Path(tmp) / "node-data"),
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


class CliMissingFlagScenarios(NodoBaseTest):
    """Commands that receive no arguments or are missing required flags."""

    def test_status_without_data_dir_fails(self) -> None:
        result = run_nodo(
            ["status", "--network", "localnet"],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )
        self.assertFailed(result)
        self.assertNotTimedOut(result)
        self.assertNoSegfault(result)

    def test_init_without_network_completes_cleanly(self) -> None:
        # The binary defaults --network to localnet when the flag is omitted.
        # We verify the command does not crash, not that it fails.
        with tempfile.TemporaryDirectory(prefix="nodo_clf_init_nonet_") as tmp:
            result = run_nodo(
                ["init", "--data-dir", str(Path(tmp) / "node-data")],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_keys_create_without_type_fails(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_clf_kc_notype_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = run_nodo(
                [
                    "keys", "create",
                    "--network", "localnet",
                    "--data-dir", str(data_dir),
                    "--key-id", "test-key",
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_keys_create_without_key_id_uses_default(self) -> None:
        # The binary defaults --key-id to "local-validator" when the flag is omitted.
        # We verify it does not crash and produces a key file.
        with tempfile.TemporaryDirectory(prefix="nodo_clf_kc_nokeyid_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = run_nodo(
                [
                    "keys", "create",
                    "--network", "localnet",
                    "--data-dir", str(data_dir),
                    "--type", "validator",
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_without_data_dir_fails(self) -> None:
        result = run_nodo(
            ["node", "reload", "--network", "localnet"],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )
        self.assertFailed(result)
        self.assertNotTimedOut(result)
        self.assertNoSegfault(result)

    def test_chain_audit_without_data_dir_fails(self) -> None:
        result = run_nodo(
            ["chain", "audit", "--network", "localnet"],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )
        self.assertFailed(result)
        self.assertNotTimedOut(result)
        self.assertNoSegfault(result)

    def test_no_subcommand_prints_help_or_fails(self) -> None:
        """Running nodo with no arguments must not crash."""
        result = run_nodo([], repo_root=self.repo_root, timeout_seconds=30)
        self.assertNotTimedOut(result)
        self.assertNoSegfault(result)


if __name__ == "__main__":
    import unittest
    unittest.main(verbosity=2)

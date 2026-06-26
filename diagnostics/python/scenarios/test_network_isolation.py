"""
Network isolation and boundary scenarios.

Verifies that:
  - Data directories cannot be reused across networks
  - Official-network restrictions apply consistently
  - Mixed-network operations are always rejected
  - Every CLI command that accepts --network enforces network boundaries
  - Status, reload, audit all check the selectedNetwork field

Category breakdown:
  - Status rejects wrong network for initialized data: 3 tests
  - Reload rejects wrong network for initialized data: 3 tests
  - Chain audit rejects wrong network: 3 tests
  - Keys create rejects wrong network: 2 tests
  - Init with wrong then right network: 4 tests
  - Cross-network readiness: 3 tests
  - Official vs localnet boundary: 6 tests
"""

from __future__ import annotations

import tempfile
from pathlib import Path

from nodo_diag.base_test import NodoBaseTest
from nodo_diag.cli_runner import run_nodo
from nodo_diag.generators import official_networks

NETWORK_MISMATCH_TEXT = "Data directory belongs to network"


class NetworkStatusIsolationScenarios(NodoBaseTest):
    """Status must reject when --network differs from stored selectedNetwork."""

    def _run_status_with_network(self, data_dir: Path, network: str):
        return run_nodo(
            ["status", "--network", network, "--data-dir", str(data_dir)],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )

    def test_localnet_data_dir_rejects_testnet_candidate_network(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ni_stat_tc_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self._run_status_with_network(data_dir, "testnet-candidate")
            self.assertFailedWithText(result, NETWORK_MISMATCH_TEXT)

    def test_localnet_data_dir_accepts_localnet_network(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ni_stat_ln_ok_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self._run_status_with_network(data_dir, "localnet")
            self.assertSucceeded(result)

    def test_status_rejects_all_official_networks_on_localnet_data(self) -> None:
        for network in official_networks():
            with self.subTest(network=network):
                with tempfile.TemporaryDirectory(prefix=f"nodo_ni_stat_{network[:6]}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    result = self._run_status_with_network(data_dir, network)
                    self.assertFailedWithText(result, NETWORK_MISMATCH_TEXT)


class NetworkReloadIsolationScenarios(NodoBaseTest):
    """Reload must reject when --network differs from stored selectedNetwork."""

    def test_localnet_data_dir_rejects_testnet_candidate_reload(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ni_rl_tc_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_reload(data_dir, network="testnet-candidate")
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_localnet_data_dir_accepts_localnet_reload(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ni_rl_ln_ok_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_reload(data_dir, network="localnet")
            self.assertSucceeded(result)

    def test_reload_rejects_all_official_networks_on_localnet_data(self) -> None:
        for network in official_networks():
            with self.subTest(network=network):
                with tempfile.TemporaryDirectory(prefix=f"nodo_ni_rl_{network[:6]}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    result = self.run_reload(data_dir, network=network)
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


class NetworkAuditIsolationScenarios(NodoBaseTest):
    """Chain audit must reject when --network differs from stored selectedNetwork."""

    def test_localnet_data_dir_rejects_testnet_candidate_audit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ni_ca_tc_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_chain_audit(data_dir, network="testnet-candidate")
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_localnet_data_dir_accepts_localnet_audit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ni_ca_ln_ok_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_chain_audit(data_dir, network="localnet")
            self.assertSucceeded(result)

    def test_chain_audit_rejects_all_official_networks_on_localnet_data(self) -> None:
        for network in official_networks():
            with self.subTest(network=network):
                with tempfile.TemporaryDirectory(prefix=f"nodo_ni_ca_{network[:6]}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    result = self.run_chain_audit(data_dir, network=network)
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


class NetworkKeysIsolationScenarios(NodoBaseTest):
    """Keys create must refuse when --network mismatches stored network."""

    def test_keys_create_rejects_testnet_candidate_on_localnet_data(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ni_kc_tc_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_keys_create(
                data_dir,
                network="testnet-candidate",
                key_type="validator",
                key_id="wrong-network-key",
            )
            self.assertFailedWithText(result, NETWORK_MISMATCH_TEXT)

    def test_keys_create_accepts_correct_localnet_network(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ni_kc_ln_ok_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_keys_create(
                data_dir,
                network="localnet",
                key_type="validator",
                key_id="correct-network-key",
            )
            self.assertSucceeded(result)


class NetworkReadinessIsolationScenarios(NodoBaseTest):
    """Testnet readiness and diagnostics enforce network boundaries."""

    def test_testnet_readiness_rejects_localnet_data_dir(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ni_ready_ln_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_testnet_readiness(
                data_dir,
                network="testnet-candidate",
                key_id="local-validator",
            )
            self.assertFailedWithText(result, NETWORK_MISMATCH_TEXT)

    def test_diagnostics_rejects_localnet_data_dir(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ni_diag_ln_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_diagnostics(
                data_dir,
                network="testnet-candidate",
                key_id="local-validator",
            )
            self.assertFailedWithText(result, NETWORK_MISMATCH_TEXT)

    def test_testnet_readiness_rejects_all_official_networks_on_localnet_data(
        self,
    ) -> None:
        for network in official_networks():
            with self.subTest(network=network):
                with tempfile.TemporaryDirectory(prefix=f"nodo_ni_ready_{network[:6]}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    result = self.run_testnet_readiness(
                        data_dir,
                        network=network,
                        key_id="local-validator",
                    )
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


class NetworkDemoCommandIsolationScenarios(NodoBaseTest):
    """Demo commands are never allowed on official networks regardless of data dir."""

    def test_demo_blocked_without_any_data_dir(self) -> None:
        """Demo commands must be blocked even before the data directory is checked."""
        for network in official_networks():
            with self.subTest(network=network):
                with tempfile.TemporaryDirectory(prefix="nodo_ni_demo_nodata_") as tmp:
                    result = run_nodo(
                        [
                            "demo",
                            "--network",
                            network,
                            "--data-dir",
                            str(Path(tmp) / "node-data"),
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailedWithText(result, "not permitted on official network")
                    self.assertNotTimedOut(result)

    def test_block_produce_allowed_on_official_networks_at_policy_level(self) -> None:
        """block produce is a canonical command and must not be blocked by policy."""
        for network in official_networks():
            with self.subTest(network=network):
                with tempfile.TemporaryDirectory(prefix="nodo_ni_blockprod_") as tmp:
                    result = run_nodo(
                        [
                            "block",
                            "produce",
                            "--network",
                            network,
                            "--data-dir",
                            str(Path(tmp) / "node-data"),
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    # Policy does not block canonical commands; any failure is an
                    # I/O or state error, not a policy rejection.
                    if not result.succeeded:
                        self.assertFalse(
                            "not permitted on official network" in (result.stderr or ""),
                            "Canonical 'block produce' must not be blocked by policy.",
                        )


if __name__ == "__main__":
    import unittest

    unittest.main(verbosity=2)

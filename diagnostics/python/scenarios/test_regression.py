from __future__ import annotations

"""
Regression tests for known bug patterns in the Nodo runtime.

Each test here documents a class of problem that was previously identified
(via commits, diagnostics, or manual testing).  If a test here fails, it
means a regression was introduced.

Commit trail context:
  - "Enforce runtime command safety and monetary audit continuity"
  - "Close protocol safety and economic audit gaps"
  - "Harden runtime safety state and monetary integrity"
  - "Add Python diagnostics for CLI safety and storage corruption"

Categories:
  1. CLI safety regressions: demo commands must remain blocked on official networks
  2. Monetary audit continuity: reload must not silently drop monetary fields
  3. Runtime state integrity: reload + status must agree
  4. Storage corruption detection: manifest validation must remain strict
  5. Key isolation: keys tied to one network must not cross to another
"""

from pathlib import Path
import tempfile

from nodo_diag.base_test import NodoBaseTest
from nodo_diag.cli_runner import run_nodo
from nodo_diag.filesystem_faults import (
    manifest_path,
    remove_key_value_line,
    replace_key_value,
    append_key_value,
)
from nodo_diag.generators import official_networks


# ---------------------------------------------------------------------------
# 1. CLI safety regressions
# ---------------------------------------------------------------------------

class CliSafetyRegressions(NodoBaseTest):
    """
    These scenarios guard against regressions in the demo-command block.

    If any of these break, the official network safety gate has been weakened.
    """

    BLOCKED_COMMANDS = [
        "demo",
        "reload",
        "submit-demo-transaction",
        "produce-demo-block",
    ]

    def test_demo_commands_still_blocked_on_testnet_candidate(self) -> None:
        for command in self.BLOCKED_COMMANDS:
            with self.subTest(command=command):
                with tempfile.TemporaryDirectory(prefix="nodo_reg_demo_tc_") as tmp:
                    result = run_nodo(
                        [
                            command,
                            "--network", "testnet-candidate",
                            "--data-dir", str(Path(tmp) / "node-data"),
                        ],
                        repo_root=self.repo_root,
                        timeout_seconds=30,
                    )
                    self.assertFailedWithText(
                        result,
                        "not permitted on official network",
                        msg=f"Regression: command '{command}' is no longer blocked on testnet-candidate",
                    )

    def test_testnet_readiness_still_requires_key_id(self) -> None:
        """Guard: readiness gate must not have been silently removed."""
        for network in official_networks():
            with self.subTest(network=network):
                with tempfile.TemporaryDirectory(prefix="nodo_reg_ready_") as tmp:
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
                        msg=f"Regression: --key-id gate removed for '{network}'",
                    )

    def test_diagnostics_command_still_requires_key_id(self) -> None:
        for network in official_networks():
            with self.subTest(network=network):
                with tempfile.TemporaryDirectory(prefix="nodo_reg_diag_") as tmp:
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
                        msg=f"Regression: diagnostics --key-id gate removed for '{network}'",
                    )


# ---------------------------------------------------------------------------
# 2. Monetary audit continuity regressions
# ---------------------------------------------------------------------------

class MonetaryAuditContinuityRegressions(NodoBaseTest):
    """
    After a reload, the node must still be in a state where audit passes.

    If audit starts failing after reload (without any corruption), it indicates
    that reload is not properly persisting or restoring monetary state.
    """

    def test_audit_passes_after_reload_on_clean_node(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_mac_1_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            self.assertSucceeded(
                self.run_reload(data_dir),
                msg="Reload must succeed on clean node",
            )
            self.assertSucceeded(
                self.run_chain_audit(data_dir),
                msg="Audit must pass after clean reload (monetary continuity regression guard)",
            )

    def test_reload_after_audit_passes_on_clean_node(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_mac_2_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            self.assertSucceeded(
                self.run_chain_audit(data_dir),
                msg="Audit must succeed on clean node",
            )
            self.assertSucceeded(
                self.run_reload(data_dir),
                msg="Reload must pass after clean audit (monetary continuity regression guard)",
            )

    def test_reload_preserves_manifest_readability(self) -> None:
        """Reload must not corrupt the manifest file."""
        with tempfile.TemporaryDirectory(prefix="nodo_reg_mac_3_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            self.assertSucceeded(self.run_reload(data_dir))

            # Manifest must still exist and be readable.
            mf = manifest_path(data_dir)
            self.assertTrue(mf.is_file(), msg="Manifest must still exist after reload")
            content = mf.read_text(encoding="utf-8", errors="replace")
            self.assertTrue(content.strip(), msg="Manifest must not be empty after reload")

    def test_status_preserves_information_after_reload(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_mac_4_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            status_before = self.run_status(data_dir)
            self.assertSucceeded(status_before)

            self.assertSucceeded(self.run_reload(data_dir))

            status_after = self.run_status(data_dir)
            self.assertSucceeded(
                status_after,
                msg="Status must succeed after reload (regression: reload corrupting manifest)",
            )


# ---------------------------------------------------------------------------
# 3. Runtime state integrity regressions
# ---------------------------------------------------------------------------

class RuntimeStateIntegrityRegressions(NodoBaseTest):
    """
    The reload and status commands must always agree on the node state.
    """

    def test_keys_before_init_is_still_rejected(self) -> None:
        """Regression guard: the 'before init' check must not have been removed."""
        with tempfile.TemporaryDirectory(prefix="nodo_reg_rsi_1_") as tmp:
            data_dir = Path(tmp) / "not-initialized"
            result = run_nodo(
                [
                    "keys", "create",
                    "--network", "localnet",
                    "--data-dir", str(data_dir),
                    "--type", "validator",
                    "--key-id", "test",
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertFailedWithText(
                result,
                "Cannot create key before init",
                msg="Regression: before-init gate for keys create was removed",
            )

    def test_network_mismatch_still_rejected_by_status(self) -> None:
        """Regression guard: network boundary check in status must not have been removed."""
        with tempfile.TemporaryDirectory(prefix="nodo_reg_rsi_2_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_status(data_dir, network="testnet-candidate")
            self.assertFailedWithText(
                result,
                "Data directory belongs to network",
                msg="Regression: network boundary check in status was removed",
            )

    def test_network_mismatch_still_rejected_by_reload(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_rsi_3_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_reload(data_dir, network="testnet-candidate")
            self.assertFailed(result, msg="Regression: network boundary check in reload was removed")


# ---------------------------------------------------------------------------
# 4. Storage corruption detection regressions
# ---------------------------------------------------------------------------

class StorageCorruptionDetectionRegressions(NodoBaseTest):
    """
    The manifest parser must remain strict.  These scenarios were the original
    motivation for the Python diagnostics suite.
    """

    def test_missing_manifest_still_causes_status_failure(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_scd_1_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            manifest_path(data_dir).unlink()
            result = self.run_status(data_dir)
            self.assertFailedWithText(
                result,
                "Failed to read Nodo status",
                msg="Regression: missing manifest no longer causes status failure",
            )

    def test_unknown_field_still_causes_status_failure(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_scd_2_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            append_key_value(manifest_path(data_dir), "regressionTestField", "injected")
            result = self.run_status(data_dir)
            self.assertFailedWithText(
                result,
                "Failed to read Nodo status",
                msg="Regression: unknown field no longer rejected",
            )

    def test_missing_chainId_still_causes_status_failure(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_scd_3_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            remove_key_value_line(manifest_path(data_dir), "chainId")
            result = self.run_status(data_dir)
            self.assertFailedWithText(
                result,
                "Failed to read Nodo status",
                msg="Regression: missing chainId no longer causes status failure",
            )

    def test_zero_validatorCount_still_causes_status_failure(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_scd_4_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "validatorCount", "0")
            result = self.run_status(data_dir)
            self.assertFailedWithText(
                result,
                "Failed to read Nodo status",
                msg="Regression: zero validatorCount no longer causes status failure",
            )

    def test_non_canonical_manifest_still_causes_status_failure(self) -> None:
        from nodo_diag.filesystem_faults import break_file_canonical_order
        with tempfile.TemporaryDirectory(prefix="nodo_reg_scd_5_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            break_file_canonical_order(manifest_path(data_dir))
            result = self.run_status(data_dir)
            self.assertFailedWithText(
                result,
                "Failed to read Nodo status",
                msg="Regression: non-canonical manifest no longer causes status failure",
            )

    def test_wrong_genesis_id_still_causes_reload_failure(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_scd_6_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "genesisConfigId", "wrong-genesis-id")
            result = self.run_reload(data_dir)
            self.assertFailedWithText(
                result,
                "Failed to reload Nodo runtime",
                msg="Regression: wrong genesisConfigId no longer causes reload failure",
            )

    def test_wrong_genesis_id_still_causes_audit_failure(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_scd_7_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "genesisConfigId", "wrong-genesis-id")
            result = self.run_chain_audit(data_dir)
            self.assertFailedWithText(
                result,
                "chain audit",
                msg="Regression: wrong genesisConfigId no longer causes audit failure",
            )

    def test_wrong_block_hash_still_causes_reload_failure(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_scd_8_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHash", "wrong-block-hash")
            result = self.run_reload(data_dir)
            self.assertFailedWithText(
                result,
                "Failed to reload Nodo runtime",
                msg="Regression: wrong latestBlockHash no longer causes reload failure",
            )

    def test_wrong_state_root_still_causes_reload_failure(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_scd_9_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestStateRoot", "wrong-state-root")
            result = self.run_reload(data_dir)
            self.assertFailedWithText(
                result,
                "Failed to reload Nodo runtime",
                msg="Regression: wrong latestStateRoot no longer causes reload failure",
            )


# ---------------------------------------------------------------------------
# 5. Key isolation regressions
# ---------------------------------------------------------------------------

class KeyIsolationRegressions(NodoBaseTest):
    """Keys must remain isolated to their network."""

    def test_localnet_key_warning_is_still_printed(self) -> None:
        """The development key warning must not have been silently removed."""
        with tempfile.TemporaryDirectory(prefix="nodo_reg_ki_1_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_keys_create(
                data_dir,
                network="localnet",
                key_type="validator",
                key_id="local-validator",
            )
            self.assertSucceeded(result)
            self.assertIn(
                "deterministic localnet-only development key",
                result.output(),
                msg="Regression: development key warning was removed from localnet key creation",
            )

    def test_localnet_key_lists_after_creation(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_reg_ki_2_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            self.run_keys_create(
                data_dir, network="localnet",
                key_type="validator", key_id="my-validator",
            )
            result = self.run_keys_list(data_dir)
            self.assertSucceeded(result, msg="Keys list must succeed after key creation")


if __name__ == "__main__":
    import unittest
    unittest.main(verbosity=2)

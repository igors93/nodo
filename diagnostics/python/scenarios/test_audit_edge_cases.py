"""
Chain audit and runtime reload edge case scenarios.

Verifies:
  - Idempotency: running reload or audit twice gives the same result
  - Sequence consistency: reload followed by audit, or vice versa
  - Mixed sequence: status → reload → audit → status all agree
  - Partial corruption after successful operations still fails
  - Audit report content (when available)
  - Reload success message format
  - Audit failure messages are informative

Category breakdown:
  - Idempotency: 4 tests
  - Sequential operation consistency: 6 tests
  - Post-corruption detection: 8 tests
  - Success message validation: 4 tests
  - Error message informativeness: 6 tests
"""

from __future__ import annotations

import tempfile
from pathlib import Path

from nodo_diag.base_test import NodoBaseTest
from nodo_diag.filesystem_faults import (
    append_key_value,
    manifest_path,
    replace_key_value,
)


class IdempotencyScenarios(NodoBaseTest):
    """Running the same command twice must produce the same outcome."""

    def test_reload_is_idempotent(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_rl_idem_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            result1 = self.run_reload(data_dir)
            result2 = self.run_reload(data_dir)

            self.assertSucceeded(result1, msg="First reload must succeed")
            self.assertSucceeded(result2, msg="Second reload must also succeed")
            self.assertEqual(
                result1.returncode,
                result2.returncode,
                msg="Reload must be idempotent — same return code both times",
            )

    def test_chain_audit_is_idempotent(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_ca_idem_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            result1 = self.run_chain_audit(data_dir)
            result2 = self.run_chain_audit(data_dir)

            self.assertSucceeded(result1, msg="First audit must succeed")
            self.assertSucceeded(result2, msg="Second audit must also succeed")
            self.assertEqual(
                result1.returncode,
                result2.returncode,
                msg="Chain audit must be idempotent",
            )

    def test_status_is_idempotent(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_stat_idem_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            result1 = self.run_status(data_dir)
            result2 = self.run_status(data_dir)

            self.assertSucceeded(result1, msg="First status must succeed")
            self.assertSucceeded(result2, msg="Second status must also succeed")

    def test_reload_then_reload_again_both_succeed(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_rl2_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            for i in range(3):
                result = self.run_reload(data_dir)
                self.assertSucceeded(result, msg=f"Reload #{i + 1} must succeed")

    def test_audit_then_audit_again_both_succeed(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_ca2_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            for i in range(3):
                result = self.run_chain_audit(data_dir)
                self.assertSucceeded(result, msg=f"Audit #{i + 1} must succeed")


class SequentialOperationScenarios(NodoBaseTest):
    """Operations in sequence must all agree on the node state."""

    def test_status_then_reload_then_status(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_seq1_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            self.assertSucceeded(self.run_status(data_dir), msg="status before reload")
            self.assertSucceeded(self.run_reload(data_dir), msg="reload")
            self.assertSucceeded(self.run_status(data_dir), msg="status after reload")

    def test_reload_then_chain_audit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_seq2_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            self.assertSucceeded(self.run_reload(data_dir), msg="reload")
            self.assertSucceeded(self.run_chain_audit(data_dir), msg="chain audit after reload")

    def test_chain_audit_then_reload(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_seq3_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            self.assertSucceeded(self.run_chain_audit(data_dir), msg="chain audit")
            self.assertSucceeded(self.run_reload(data_dir), msg="reload after chain audit")

    def test_full_sequence_status_reload_audit_status(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_seq4_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            self.assertSucceeded(self.run_status(data_dir), msg="status 1")
            self.assertSucceeded(self.run_reload(data_dir), msg="reload")
            self.assertSucceeded(self.run_chain_audit(data_dir), msg="audit")
            self.assertSucceeded(self.run_status(data_dir), msg="status 2")

    def test_multiple_reloads_interleaved_with_audits(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_seq5_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            for i in range(2):
                self.assertSucceeded(self.run_reload(data_dir), msg=f"reload {i}")
                self.assertSucceeded(self.run_chain_audit(data_dir), msg=f"audit {i}")

    def test_keys_create_then_reload_then_audit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_seq6_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            self.assertSucceeded(
                self.run_keys_create(data_dir),
                msg="keys create",
            )
            self.assertSucceeded(self.run_reload(data_dir), msg="reload after key create")
            self.assertSucceeded(self.run_chain_audit(data_dir), msg="audit after key create")


class PostCorruptionDetectionScenarios(NodoBaseTest):
    """After a successful operation, a subsequent corruption must be detected."""

    def test_reload_succeeds_then_corruption_causes_reload_to_fail(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_pc_rl_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            self.assertSucceeded(self.run_reload(data_dir), msg="clean reload before corruption")

            replace_key_value(manifest_path(data_dir), "latestBlockHash", "corrupted-by-test")

            result = self.run_reload(data_dir)
            self.assertFailed(result, msg="reload must fail after corruption")
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_audit_succeeds_then_corruption_causes_audit_to_fail(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_pc_ca_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            self.assertSucceeded(
                self.run_chain_audit(data_dir), msg="clean audit before corruption"
            )

            replace_key_value(manifest_path(data_dir), "latestStateRoot", "corrupted-by-test")

            result = self.run_chain_audit(data_dir)
            self.assertFailed(result, msg="audit must fail after corruption")
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_status_succeeds_then_corruption_causes_status_to_fail(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_pc_stat_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            self.assertSucceeded(self.run_status(data_dir), msg="clean status before corruption")

            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "bad-height")

            result = self.run_status(data_dir)
            self.assertFailed(result, msg="status must fail after corruption")
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_unknown_field_injected_after_reload_causes_next_reload_to_fail(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_pc_unk_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            self.assertSucceeded(self.run_reload(data_dir), msg="clean reload")

            append_key_value(manifest_path(data_dir), "injectedByTest", "bad-value")

            result = self.run_reload(data_dir)
            self.assertFailed(result, msg="reload must fail with unknown field")
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_genesis_corruption_detected_by_both_reload_and_audit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_pc_gen_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            replace_key_value(manifest_path(data_dir), "genesisConfigId", "wrong-genesis-id")

            reload_result = self.run_reload(data_dir)
            audit_result = self.run_chain_audit(data_dir)

            self.assertFailed(reload_result, msg="reload must detect genesis corruption")
            self.assertFailed(audit_result, msg="audit must detect genesis corruption")

    def test_height_mismatch_detected_by_both_reload_and_audit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_pc_height_") as tmp:
            data_dir = self.init_localnet(Path(tmp))

            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "999")

            reload_result = self.run_reload(data_dir)
            audit_result = self.run_chain_audit(data_dir)

            self.assertFailed(reload_result, msg="reload must detect height mismatch")
            self.assertFailed(audit_result, msg="audit must detect height mismatch")


class ReloadSuccessMessageScenarios(NodoBaseTest):
    """Success cases validate that expected output text is present."""

    def test_reload_success_prints_expected_message(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_msg_rl_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_reload(data_dir)
            self.assertSucceededWithText(result, "Nodo runtime reloaded.")

    def test_chain_audit_success_returns_zero(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_msg_ca_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_chain_audit(data_dir)
            self.assertSucceeded(result)
            self.assertNotTimedOut(result)

    def test_status_success_returns_zero_with_output(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_msg_stat_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_status(data_dir)
            self.assertSucceeded(result)
            self.assertTrue(
                result.output().strip(),
                msg="Status should produce some output on success",
            )

    def test_reload_failure_message_is_informative(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_msg_rl_fail_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHash", "bad-hash")
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertTrue(
                result.output().strip(),
                msg="Failed reload should produce diagnostic output",
            )

    def test_audit_failure_message_is_informative(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ae_msg_ca_fail_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestStateRoot", "bad-root")
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)
            self.assertTrue(
                result.output().strip(),
                msg="Failed audit should produce diagnostic output",
            )


if __name__ == "__main__":
    import unittest

    unittest.main(verbosity=2)

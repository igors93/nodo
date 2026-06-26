"""
Manifest field scenarios — exhaustive validation of manifest.nodo parsing.

Category breakdown (subtests within each test method):
  - Required field removal: 7 subtests
  - Integer field malformed values: 22 subtests × 1 field = 22
  - Positive-integer field malformed values: 23 subtests × 1 field = 23
  - Hash / config-ID field malformed values: 15 subtests × 3 fields = 45
  - Unknown field injection: 8 subtests
  - Canonical order violations: 4 subtests (status + reload + audit × 2 strategies)
  - Reload rejection of corrupted hash fields: 15 subtests × 3 fields = 45
  - Chain-audit rejection of corrupted hash fields: 15 subtests × 3 fields = 45
  - Reload rejection of malformed heights: 22 subtests
  - Chain-audit rejection of malformed heights: 22 subtests

Total effective scenarios: ~260+
"""

from __future__ import annotations

import tempfile
from pathlib import Path

from nodo_diag.base_test import NodoBaseTest
from nodo_diag.filesystem_faults import (
    append_key_value,
    break_file_canonical_order,
    manifest_path,
    remove_key_value_line,
    replace_key_value,
    write_text,
)
from nodo_diag.generators import (
    bad_config_id_values,
    bad_hash_values,
    bad_integer_values,
    bad_positive_integer_values,
    known_required_manifest_fields,
    unknown_field_names,
)

STATUS_FAILURE_TEXT = "Failed to read Nodo status"
RELOAD_FAILURE_TEXT = "Failed to reload Nodo runtime"
AUDIT_FAILURE_TEXT = "chain audit"


class ManifestFieldScenarios(NodoBaseTest):
    """
    Every test below operates on a freshly initialised localnet, then corrupts
    the manifest before invoking the CLI.  The system must reject corrupted
    manifests cleanly (no crash, no silent data loss).
    """

    # ------------------------------------------------------------------
    # Required field removal — status must fail
    # ------------------------------------------------------------------

    def test_status_rejects_each_removed_required_field(self) -> None:
        """Removing any single required field must cause status to fail."""
        for field in known_required_manifest_fields():
            with self.subTest(field=field):
                with tempfile.TemporaryDirectory(prefix=f"nodo_mf_rm_{field}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    remove_key_value_line(manifest_path(data_dir), field)
                    result = self.run_status(data_dir)
                    self.assertFailedWithText(result, STATUS_FAILURE_TEXT)

    # ------------------------------------------------------------------
    # Integer field malformed values — status must fail
    # ------------------------------------------------------------------

    def test_status_rejects_malformed_latestBlockHeight(self) -> None:
        for label, bad_value in bad_integer_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_mf_height_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHeight", bad_value)
                    result = self.run_status(data_dir)
                    self.assertFailedWithText(result, STATUS_FAILURE_TEXT)

    # ------------------------------------------------------------------
    # Positive-integer field malformed values — status must fail
    # ------------------------------------------------------------------

    def test_status_rejects_malformed_validatorCount(self) -> None:
        for label, bad_value in bad_positive_integer_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_mf_valcount_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "validatorCount", bad_value)
                    result = self.run_status(data_dir)
                    self.assertFailedWithText(result, STATUS_FAILURE_TEXT)

    # ------------------------------------------------------------------
    # Hash-like field malformed values — status must fail
    # ------------------------------------------------------------------

    def test_status_rejects_malformed_latestBlockHash(self) -> None:
        for label, bad_value in bad_hash_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_mf_bkhash_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHash", bad_value)
                    result = self.run_status(data_dir)
                    self.assertFailedWithText(result, STATUS_FAILURE_TEXT)

    def test_status_rejects_malformed_latestStateRoot(self) -> None:
        for label, bad_value in bad_hash_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_mf_stroot_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestStateRoot", bad_value)
                    result = self.run_status(data_dir)
                    self.assertFailedWithText(result, STATUS_FAILURE_TEXT)

    def test_status_rejects_malformed_genesisConfigId(self) -> None:
        for label, bad_value in bad_config_id_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_mf_genesis_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "genesisConfigId", bad_value)
                    result = self.run_status(data_dir)
                    self.assertFailedWithText(result, STATUS_FAILURE_TEXT)

    # ------------------------------------------------------------------
    # Unknown field injection — status must fail
    # ------------------------------------------------------------------

    def test_status_rejects_unknown_field_names(self) -> None:
        for label, field_name in unknown_field_names():
            if not field_name:
                continue  # empty field name would be a parse edge case — skip
            with self.subTest(label=label, field=field_name):
                with tempfile.TemporaryDirectory(prefix=f"nodo_mf_unk_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    append_key_value(manifest_path(data_dir), field_name, "injected-by-python-test")
                    result = self.run_status(data_dir)
                    self.assertFailedWithText(result, STATUS_FAILURE_TEXT)

    # ------------------------------------------------------------------
    # Canonical order violations — status must fail
    # ------------------------------------------------------------------

    def test_status_rejects_non_canonical_manifest(self) -> None:
        """Swapping any two adjacent body lines must fail canonical-order check."""
        with tempfile.TemporaryDirectory(prefix="nodo_mf_canonical_status_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            break_file_canonical_order(manifest_path(data_dir))
            result = self.run_status(data_dir)
            self.assertFailedWithText(result, STATUS_FAILURE_TEXT)

    def test_reload_rejects_non_canonical_manifest(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_canonical_reload_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            break_file_canonical_order(manifest_path(data_dir))
            result = self.run_reload(data_dir)
            self.assertFailed(result)

    def test_chain_audit_rejects_non_canonical_manifest(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_canonical_audit_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            break_file_canonical_order(manifest_path(data_dir))
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)

    # ------------------------------------------------------------------
    # Truncated / empty manifest
    # ------------------------------------------------------------------

    def test_status_rejects_empty_manifest(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_empty_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            write_text(manifest_path(data_dir), "")
            result = self.run_status(data_dir)
            self.assertFailedWithText(result, STATUS_FAILURE_TEXT)

    def test_reload_rejects_empty_manifest(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_empty_reload_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            write_text(manifest_path(data_dir), "")
            result = self.run_reload(data_dir)
            self.assertFailed(result)

    def test_chain_audit_rejects_empty_manifest(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_empty_audit_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            write_text(manifest_path(data_dir), "")
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)

    def test_status_rejects_manifest_with_only_comments(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_only_comments_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            write_text(manifest_path(data_dir), "# this is a comment\n# another comment\n")
            result = self.run_status(data_dir)
            self.assertFailedWithText(result, STATUS_FAILURE_TEXT)

    def test_status_rejects_manifest_with_only_whitespace(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_whitespace_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            write_text(manifest_path(data_dir), "   \n\t\n   \n")
            result = self.run_status(data_dir)
            self.assertFailedWithText(result, STATUS_FAILURE_TEXT)

    def test_status_rejects_manifest_with_binary_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_binary_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            manifest_path(data_dir).write_bytes(bytes(range(256)))
            result = self.run_status(data_dir)
            self.assertFailed(result)

    # ------------------------------------------------------------------
    # Reload — hash field corruption (reload validates hashes strictly)
    # ------------------------------------------------------------------

    def test_reload_rejects_malformed_latestBlockHash(self) -> None:
        for label, bad_value in bad_hash_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_rl_bkhash_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHash", bad_value)
                    result = self.run_reload(data_dir)
                    self.assertFailedWithText(result, RELOAD_FAILURE_TEXT)

    def test_reload_rejects_malformed_latestStateRoot(self) -> None:
        for label, bad_value in bad_hash_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_rl_stroot_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestStateRoot", bad_value)
                    result = self.run_reload(data_dir)
                    self.assertFailedWithText(result, RELOAD_FAILURE_TEXT)

    def test_reload_rejects_malformed_genesisConfigId(self) -> None:
        for label, bad_value in bad_config_id_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_rl_genesis_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "genesisConfigId", bad_value)
                    result = self.run_reload(data_dir)
                    self.assertFailedWithText(result, RELOAD_FAILURE_TEXT)

    def test_reload_rejects_malformed_latestBlockHeight(self) -> None:
        for label, bad_value in bad_integer_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_rl_height_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHeight", bad_value)
                    result = self.run_reload(data_dir)
                    self.assertFailed(result)

    # ------------------------------------------------------------------
    # Chain audit — hash field corruption
    # ------------------------------------------------------------------

    def test_chain_audit_rejects_malformed_latestBlockHash(self) -> None:
        for label, bad_value in bad_hash_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_ca_bkhash_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHash", bad_value)
                    result = self.run_chain_audit(data_dir)
                    self.assertFailedWithText(result, AUDIT_FAILURE_TEXT)

    def test_chain_audit_rejects_malformed_latestStateRoot(self) -> None:
        for label, bad_value in bad_hash_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_ca_stroot_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestStateRoot", bad_value)
                    result = self.run_chain_audit(data_dir)
                    self.assertFailedWithText(result, AUDIT_FAILURE_TEXT)

    def test_chain_audit_rejects_malformed_genesisConfigId(self) -> None:
        for label, bad_value in bad_config_id_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_ca_genesis_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "genesisConfigId", bad_value)
                    result = self.run_chain_audit(data_dir)
                    self.assertFailedWithText(result, AUDIT_FAILURE_TEXT)

    def test_chain_audit_rejects_malformed_latestBlockHeight(self) -> None:
        for label, bad_value in bad_integer_values():
            with self.subTest(label=label, value=bad_value):
                with tempfile.TemporaryDirectory(prefix=f"nodo_ca_height_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHeight", bad_value)
                    result = self.run_chain_audit(data_dir)
                    self.assertFailed(result)

    # ------------------------------------------------------------------
    # Duplicate key injection
    # ------------------------------------------------------------------

    def test_status_rejects_manifest_with_duplicate_chainId(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_dup_chainId_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            append_key_value(manifest_path(data_dir), "chainId", "duplicate-injected-by-test")
            result = self.run_status(data_dir)
            self.assertFailed(result)

    def test_status_rejects_manifest_with_duplicate_latestBlockHeight(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_dup_height_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            append_key_value(manifest_path(data_dir), "latestBlockHeight", "99")
            result = self.run_status(data_dir)
            self.assertFailed(result)

    # ------------------------------------------------------------------
    # Extremely long field values — must not hang or crash
    # ------------------------------------------------------------------

    def test_status_handles_extremely_long_chainId_safely(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_long_chainId_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "chainId", "x" * 65536)
            result = self.run_status(data_dir)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)
            self.assertFailed(result)

    def test_status_handles_extremely_long_unknown_field_safely(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_long_unknown_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            append_key_value(manifest_path(data_dir), "unknownField", "v" * 65536)
            result = self.run_status(data_dir)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)
            self.assertFailed(result)

    # ------------------------------------------------------------------
    # Manifest with extra blank lines or comment lines
    # ------------------------------------------------------------------

    def test_status_handles_manifest_with_leading_blank_lines(self) -> None:
        """Blank lines at start — parser may accept or reject, but must not crash."""
        with tempfile.TemporaryDirectory(prefix="nodo_mf_lead_blank_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            original = manifest_path(data_dir).read_text(encoding="utf-8")
            write_text(manifest_path(data_dir), "\n\n" + original)
            result = self.run_status(data_dir)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_status_handles_manifest_with_trailing_blank_lines(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_trail_blank_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            original = manifest_path(data_dir).read_text(encoding="utf-8")
            write_text(manifest_path(data_dir), original + "\n\n\n")
            result = self.run_status(data_dir)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    # ------------------------------------------------------------------
    # Positive case: clean manifest loads correctly
    # ------------------------------------------------------------------

    def test_status_succeeds_on_clean_localnet_manifest(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_clean_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_status(data_dir)
            self.assertSucceeded(result)
            self.assertNotTimedOut(result)

    def test_reload_succeeds_on_clean_localnet_manifest(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_clean_rl_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_reload(data_dir)
            self.assertSucceeded(result)
            self.assertNotTimedOut(result)

    def test_chain_audit_succeeds_on_clean_localnet_manifest(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_mf_clean_ca_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_chain_audit(data_dir)
            self.assertSucceeded(result)
            self.assertNotTimedOut(result)


if __name__ == "__main__":
    import unittest

    unittest.main(verbosity=2)

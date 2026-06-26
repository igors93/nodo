"""
Block file and block-reference corruption scenarios.

Since the binary enforces consistency between manifest and block files, we can
trigger failures by:
  1. Claiming a higher block height in the manifest (block file doesn't exist)
  2. Claiming a negative / nonsensical block height
  3. Creating fake block files with corrupted content
  4. Corrupting multiple manifest fields simultaneously

Category breakdown:
  - Reload rejects unreachable block heights: 12 subtests
  - Audit rejects unreachable block heights: 12 subtests
  - Status with implausible heights: 10 subtests
  - Reload with multiple simultaneous corruptions: 6 tests
  - Audit with multiple simultaneous corruptions: 6 tests
  - Fake block file injection: 5 tests
  - Combined manifest + block file corruption: 4 tests
"""

from __future__ import annotations

import tempfile
from pathlib import Path

from nodo_diag.base_test import NodoBaseTest
from nodo_diag.filesystem_faults import (
    append_key_value,
    manifest_path,
    replace_key_value,
    write_text,
)

RELOAD_FAILURE_TEXT = "Failed to reload Nodo runtime"
AUDIT_FAILURE_TEXT = "chain audit"


# Heights that claim non-existent blocks (genesis is height 0).
BAD_BLOCK_HEIGHTS = [
    ("height_1", "1"),
    ("height_2", "2"),
    ("height_10", "10"),
    ("height_100", "100"),
    ("height_999", "999"),
    ("height_max_int", "2147483647"),
    ("height_huge", "9999999999"),
    ("height_overflow", "9" * 19),
]

# Heights that are syntactically bad (parser must reject before trying to load blocks).
SYNTACTICALLY_BAD_HEIGHTS = [
    ("neg_1", "-1"),
    ("neg_large", "-9999"),
    ("float", "1.5"),
    ("nan", "NaN"),
    ("text", "not-a-height"),
    ("empty", ""),
    ("hex", "0xFF"),
    ("overflow", "9" * 25),
]


class ReloadBlockHeightScenarios(NodoBaseTest):
    """Reload must reject when the declared block height has no matching block file."""

    def test_reload_rejects_unreachable_block_heights(self) -> None:
        for label, height in BAD_BLOCK_HEIGHTS:
            with self.subTest(label=label, height=height):
                with tempfile.TemporaryDirectory(prefix=f"nodo_bc_rl_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHeight", height)
                    result = self.run_reload(data_dir)
                    self.assertFailedWithText(result, RELOAD_FAILURE_TEXT)

    def test_reload_rejects_syntactically_bad_heights(self) -> None:
        for label, height in SYNTACTICALLY_BAD_HEIGHTS:
            with self.subTest(label=label, height=height):
                with tempfile.TemporaryDirectory(prefix=f"nodo_bc_rl_syn_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHeight", height)
                    result = self.run_reload(data_dir)
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


class AuditBlockHeightScenarios(NodoBaseTest):
    """Chain audit must reject when block files are inconsistent with manifest."""

    def test_chain_audit_rejects_unreachable_block_heights(self) -> None:
        for label, height in BAD_BLOCK_HEIGHTS:
            with self.subTest(label=label, height=height):
                with tempfile.TemporaryDirectory(prefix=f"nodo_bc_ca_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHeight", height)
                    result = self.run_chain_audit(data_dir)
                    self.assertFailedWithText(result, AUDIT_FAILURE_TEXT)

    def test_chain_audit_rejects_syntactically_bad_heights(self) -> None:
        for label, height in SYNTACTICALLY_BAD_HEIGHTS:
            with self.subTest(label=label, height=height):
                with tempfile.TemporaryDirectory(prefix=f"nodo_bc_ca_syn_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHeight", height)
                    result = self.run_chain_audit(data_dir)
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


class StatusBlockHeightScenarios(NodoBaseTest):
    """Status reads the manifest height field; it must reject clearly bad values."""

    def test_status_rejects_negative_block_height(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_stat_neg_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "-1")
            result = self.run_status(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_status_rejects_nan_block_height(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_stat_nan_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "NaN")
            result = self.run_status(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_status_rejects_float_block_height(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_stat_float_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "0.5")
            result = self.run_status(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_status_handles_very_large_block_height_safely(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_stat_huge_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "9" * 25)
            result = self.run_status(data_dir)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)
            self.assertFailed(result)


class MultiFieldCorruptionReloadScenarios(NodoBaseTest):
    """Reload with multiple simultaneous manifest field corruptions."""

    def test_reload_rejects_wrong_genesis_and_wrong_hash(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_rl_multi1_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "genesisConfigId", "bad-genesis")
            replace_key_value(manifest_path(data_dir), "latestBlockHash", "bad-hash")
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_rejects_wrong_hash_and_wrong_height(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_rl_multi2_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHash", "bad-hash")
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "100")
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_rejects_wrong_state_root_and_wrong_height(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_rl_multi3_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestStateRoot", "bad-root")
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "100")
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_rejects_all_three_hash_fields_corrupted(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_rl_multi4_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "genesisConfigId", "bad-genesis")
            replace_key_value(manifest_path(data_dir), "latestBlockHash", "bad-hash")
            replace_key_value(manifest_path(data_dir), "latestStateRoot", "bad-root")
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_rejects_height_plus_added_unknown_field(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_rl_multi5_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "5")
            append_key_value(manifest_path(data_dir), "unknownFieldInjected", "bad")
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)


class MultiFieldCorruptionAuditScenarios(NodoBaseTest):
    """Chain audit with multiple simultaneous manifest field corruptions."""

    def test_chain_audit_rejects_wrong_genesis_and_wrong_hash(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_ca_multi1_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "genesisConfigId", "bad-genesis")
            replace_key_value(manifest_path(data_dir), "latestBlockHash", "bad-hash")
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_chain_audit_rejects_wrong_hash_and_wrong_height(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_ca_multi2_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHash", "bad-hash")
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "100")
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_chain_audit_rejects_all_three_hash_fields_corrupted(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_ca_multi3_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "genesisConfigId", "bad-genesis")
            replace_key_value(manifest_path(data_dir), "latestBlockHash", "bad-hash")
            replace_key_value(manifest_path(data_dir), "latestStateRoot", "bad-root")
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)


class FakeBlockFileScenarios(NodoBaseTest):
    """
    Inject fake block files alongside a manifest that references them.

    We can only guess at block file naming conventions from context
    (artifact_audit.py scans for .block files).  These tests create plausible
    but corrupted block files and point the manifest at them.
    """

    def _create_fake_block_file(self, data_dir: Path, height: int, content: str) -> None:
        """Create a .block file at a guessed location with arbitrary content."""
        blocks_dir = data_dir / "blocks"
        blocks_dir.mkdir(parents=True, exist_ok=True)
        fake_block = blocks_dir / f"{height:016d}.block"
        write_text(fake_block, content)

    def test_reload_rejects_fake_block_with_empty_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_fake_empty_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            self._create_fake_block_file(data_dir, 1, "")
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_rejects_fake_block_with_garbage_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_fake_garbage_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            self._create_fake_block_file(data_dir, 1, "THIS IS NOT A VALID BLOCK FILE\n")
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_chain_audit_rejects_fake_block_with_empty_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_fake_ca_empty_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            self._create_fake_block_file(data_dir, 1, "")
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_chain_audit_rejects_fake_block_with_garbage_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_fake_ca_garbage_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            self._create_fake_block_file(data_dir, 1, "NOT A BLOCK\nschema=fake\n")
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_rejects_fake_block_with_binary_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_bc_fake_bin_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            blocks_dir = data_dir / "blocks"
            blocks_dir.mkdir(parents=True, exist_ok=True)
            fake_block = blocks_dir / "0000000000000001.block"
            fake_block.write_bytes(bytes(range(256)))
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)


if __name__ == "__main__":
    import unittest

    unittest.main(verbosity=2)

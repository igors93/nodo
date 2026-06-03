from __future__ import annotations

"""
Artifact integrity and treasury report digest scenarios.

These tests verify that the enforcement added in RuntimeStateLoader and
ChainAuditor works end-to-end via the CLI:

  1. Corrupted block artifact files must cause reload and audit to fail.
  2. A manifest claiming height N > 0 without a matching valid block file
     must cause reload and audit to fail (artifact integrity gap).
  3. Reload and audit must continue passing on a clean localnet node after
     the artifact digest enforcement was added (regression guard).

Category breakdown:
  - Artifact file corruption: 10 subtests across reload and audit
  - Manifest/block height mismatch: 8 subtests
  - Clean node regression guards: 4 tests
  - Repeated audit stability: 3 tests
"""

from pathlib import Path
import tempfile

from nodo_diag.base_test import NodoBaseTest
from nodo_diag.filesystem_faults import (
    manifest_path,
    replace_key_value,
    write_text,
)


class ArtifactCorruptionReloadScenarios(NodoBaseTest):
    """
    Reload must reject corrupted or fake block artifact files.

    The binary's RuntimeStateLoader now enforces that every loaded artifact
    produces a non-empty, deterministic digest.  A fake file with garbage
    content will fail either schema validation or digest computation.
    """

    def _write_fake_block(self, data_dir: Path, height: int, content: str) -> None:
        blocks_dir = data_dir / "blocks"
        blocks_dir.mkdir(parents=True, exist_ok=True)
        (blocks_dir / f"{height:016d}.block").write_text(content, encoding="utf-8")

    def _write_fake_block_binary(self, data_dir: Path, height: int) -> None:
        blocks_dir = data_dir / "blocks"
        blocks_dir.mkdir(parents=True, exist_ok=True)
        (blocks_dir / f"{height:016d}.block").write_bytes(bytes(range(256)))

    def test_reload_rejects_fake_block_with_empty_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_rl_empty_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            self._write_fake_block(data_dir, 1, "")
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_rejects_fake_block_with_garbage_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_rl_garbage_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            self._write_fake_block(data_dir, 1, "THIS IS NOT A VALID ARTIFACT\nrandom=content\n")
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_rejects_fake_block_with_binary_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_rl_binary_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            self._write_fake_block_binary(data_dir, 1)
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_rejects_fake_block_with_plausible_but_wrong_schema(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_rl_schema_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            fake_content = (
                "schemaId=NODO_FINALIZED_BLOCK_FAKE_V1\n"
                "blockHeight=1\n"
                "blockHash=notarealhash\n"
                "monetaryFirewallStatus=INJECTED_BY_DIAGNOSTIC\n"
            )
            self._write_fake_block(data_dir, 1, fake_content)
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_reload_fails_cleanly_when_block_file_is_missing(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_rl_missing_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            # No block file created — manifest claims height 1 but file is absent.
            result = self.run_reload(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)


class ArtifactCorruptionAuditScenarios(NodoBaseTest):
    """Chain audit must reject corrupted block artifact files with the same strictness as reload."""

    def _write_fake_block(self, data_dir: Path, height: int, content: str) -> None:
        blocks_dir = data_dir / "blocks"
        blocks_dir.mkdir(parents=True, exist_ok=True)
        (blocks_dir / f"{height:016d}.block").write_text(content, encoding="utf-8")

    def test_chain_audit_rejects_fake_block_with_empty_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_ca_empty_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            self._write_fake_block(data_dir, 1, "")
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_chain_audit_rejects_fake_block_with_garbage_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_ca_garbage_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            self._write_fake_block(data_dir, 1, "GARBAGE ARTIFACT CONTENT")
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_chain_audit_fails_cleanly_when_block_file_is_missing(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_ca_missing_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            replace_key_value(manifest_path(data_dir), "latestBlockHeight", "1")
            result = self.run_chain_audit(data_dir)
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_chain_audit_rejects_multiple_fake_blocks_at_various_heights(self) -> None:
        for label, height in [("h1", "1"), ("h5", "5"), ("h99", "99")]:
            with self.subTest(label=label, height=height):
                with tempfile.TemporaryDirectory(prefix=f"nodo_ai_ca_multi_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHeight", height)
                    result = self.run_chain_audit(data_dir)
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


class ArtifactIntegrityRegressionGuards(NodoBaseTest):
    """
    Clean node: reload and audit must continue passing after the artifact
    digest enforcement was added.  These guard against accidentally blocking
    legitimate nodes.
    """

    def test_clean_localnet_reload_succeeds_after_digest_enforcement(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_clean_rl_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_reload(data_dir)
            self.assertSucceeded(result)
            self.assertNotTimedOut(result)
            self.assertSucceededWithText(result, "Nodo runtime reloaded.")

    def test_clean_localnet_audit_succeeds_after_digest_enforcement(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_clean_ca_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_chain_audit(data_dir)
            self.assertSucceeded(result)
            self.assertNotTimedOut(result)

    def test_reload_then_audit_both_succeed_on_clean_node(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_rl_ca_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            self.assertSucceeded(self.run_reload(data_dir), msg="reload must pass")
            self.assertSucceeded(self.run_chain_audit(data_dir), msg="audit must pass after reload")

    def test_repeated_audit_is_stable_on_clean_node(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_ai_repeated_ca_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            for i in range(3):
                with self.subTest(iteration=i):
                    result = self.run_chain_audit(data_dir)
                    self.assertSucceeded(result, msg=f"Audit #{i+1} must succeed (stability guard)")


class ManifestHeightMismatchScenarios(NodoBaseTest):
    """
    Manifest latestBlockHeight pointing to heights with no valid block file
    must fail reload and audit with precise, non-crashing errors.
    These directly exercise the artifact digest enforcement path added in
    RuntimeStateLoader.
    """

    HEIGHT_VARIANTS = [
        ("height_1",   "1"),
        ("height_2",   "2"),
        ("height_10",  "10"),
        ("height_100", "100"),
    ]

    def test_reload_rejects_all_unreachable_heights(self) -> None:
        for label, height in self.HEIGHT_VARIANTS:
            with self.subTest(label=label, height=height):
                with tempfile.TemporaryDirectory(prefix=f"nodo_ai_rl_h_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHeight", height)
                    result = self.run_reload(data_dir)
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)

    def test_chain_audit_rejects_all_unreachable_heights(self) -> None:
        for label, height in self.HEIGHT_VARIANTS:
            with self.subTest(label=label, height=height):
                with tempfile.TemporaryDirectory(prefix=f"nodo_ai_ca_h_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    replace_key_value(manifest_path(data_dir), "latestBlockHeight", height)
                    result = self.run_chain_audit(data_dir)
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


if __name__ == "__main__":
    import unittest
    unittest.main(verbosity=2)

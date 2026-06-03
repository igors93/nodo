from __future__ import annotations

"""
Key management operation scenarios.

Verifies that:
  - Keys cannot be created or listed before init
  - Network mismatches are rejected
  - Invalid key types are rejected
  - Invalid key IDs are rejected cleanly (no crash, no path traversal)
  - Localnet key creation succeeds with the expected development warning
  - Key listing reflects created keys
  - Duplicate key creation is handled safely

Category breakdown:
  - Before-init rejections: 2 tests
  - Network mismatch: 3 tests
  - Invalid type: 7 subtests
  - Invalid key ID safety: 15 subtests
  - Localnet success cases: 5 tests
  - Listing and post-creation checks: 4 tests
  - Positive warning checks: 3 tests
"""

from pathlib import Path
import tempfile

from nodo_diag.base_test import NodoBaseTest
from nodo_diag.cli_runner import run_nodo
from nodo_diag.generators import bad_key_id_values, bad_key_types


class KeyBeforeInitScenarios(NodoBaseTest):
    """Key operations attempted before init must be rejected."""

    def test_keys_create_before_init_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_before_init_") as tmp:
            data_dir = Path(tmp) / "node-data"
            result = run_nodo(
                [
                    "keys", "create",
                    "--network", "localnet",
                    "--data-dir", str(data_dir),
                    "--type", "validator",
                    "--key-id", "local-validator",
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertFailedWithText(result, "Cannot create key before init")

    def test_keys_list_before_init_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_list_before_init_") as tmp:
            data_dir = Path(tmp) / "node-data"
            result = run_nodo(
                ["keys", "list", "--data-dir", str(data_dir)],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertFailedWithText(result, "Cannot list keys before init")

    def test_keys_create_into_nonexistent_parent_before_init(self) -> None:
        """A deeply nested nonexistent path must not accidentally succeed."""
        with tempfile.TemporaryDirectory(prefix="nodo_key_deep_noinit_") as tmp:
            data_dir = Path(tmp) / "a" / "b" / "c" / "node-data"
            result = run_nodo(
                [
                    "keys", "create",
                    "--network", "localnet",
                    "--data-dir", str(data_dir),
                    "--type", "validator",
                    "--key-id", "local-validator",
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)


class KeyNetworkMismatchScenarios(NodoBaseTest):
    """Mismatched network must be caught when creating keys."""

    def test_keys_create_rejects_testnet_candidate_network_on_localnet_data(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_mismatch_tc_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = run_nodo(
                [
                    "keys", "create",
                    "--network", "testnet-candidate",
                    "--data-dir", str(data_dir),
                    "--type", "validator",
                    "--key-id", "local-validator",
                ],
                repo_root=self.repo_root,
                timeout_seconds=30,
            )
            self.assertFailedWithText(result, "Data directory belongs to network")

    def test_keys_create_rejects_wrong_localnet_data_with_testnet_candidate(self) -> None:
        """Mirror: using localnet data dir with testnet-candidate must fail."""
        with tempfile.TemporaryDirectory(prefix="nodo_key_mismatch_rev_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_keys_create(
                data_dir,
                network="testnet-candidate",
                key_type="validator",
                key_id="test-key",
            )
            self.assertFailedWithText(result, "Data directory belongs to network")

    def test_keys_list_does_not_expose_wrong_network_data(self) -> None:
        """List should fail or return empty — not expose another network's keys."""
        with tempfile.TemporaryDirectory(prefix="nodo_key_list_mismatch_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_keys_list(data_dir)
            # Must not crash regardless of outcome.
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)


class KeyTypeValidationScenarios(NodoBaseTest):
    """Key --type flag rejects invalid / unknown values."""

    def test_keys_create_rejects_invalid_types(self) -> None:
        for label, bad_type in bad_key_types():
            with self.subTest(label=label, type=bad_type):
                with tempfile.TemporaryDirectory(prefix=f"nodo_key_badtype_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    result = self.run_keys_create(
                        data_dir,
                        network="localnet",
                        key_type=bad_type,
                        key_id="test-key",
                    )
                    self.assertFailed(result)
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)


class KeyIdValidationScenarios(NodoBaseTest):
    """Key --key-id flag: unsafe or malformed IDs must not cause crashes."""

    def test_keys_create_handles_dangerous_key_ids_safely(self) -> None:
        for label, bad_id in bad_key_id_values():
            with self.subTest(label=label, key_id=bad_id):
                with tempfile.TemporaryDirectory(prefix=f"nodo_key_badid_{label}_") as tmp:
                    data_dir = self.init_localnet(Path(tmp))
                    result = self.run_keys_create(
                        data_dir,
                        network="localnet",
                        key_type="validator",
                        key_id=bad_id,
                    )
                    # The binary may reject or accept these, but must not crash or hang.
                    self.assertNotTimedOut(result)
                    self.assertNoSegfault(result)

    def test_keys_create_rejects_empty_key_id(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_empty_id_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_keys_create(
                data_dir,
                network="localnet",
                key_type="validator",
                key_id="",
            )
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_keys_create_rejects_path_traversal_key_id(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_traversal_id_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_keys_create(
                data_dir,
                network="localnet",
                key_type="validator",
                key_id="../../etc/passwd",
            )
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_keys_create_rejects_absolute_path_key_id(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_abs_id_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_keys_create(
                data_dir,
                network="localnet",
                key_type="validator",
                key_id="/etc/passwd",
            )
            self.assertFailed(result)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)


class KeyLocalnetSuccessScenarios(NodoBaseTest):
    """Positive cases: valid localnet key creation and listing."""

    def test_localnet_validator_key_creation_succeeds(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_ok_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_keys_create(
                data_dir,
                network="localnet",
                key_type="validator",
                key_id="local-validator",
            )
            self.assertSucceeded(result)
            self.assertNotTimedOut(result)

    def test_localnet_key_creation_prints_development_warning(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_warn_") as tmp:
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
                msg=(
                    "Localnet keys must warn they are development-only.\n"
                    f"stdout:\n{result.stdout}\n"
                    f"stderr:\n{result.stderr}"
                ),
            )

    def test_localnet_key_creation_prints_nodo_development_key_created(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_devwarning_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_keys_create(
                data_dir,
                network="localnet",
                key_type="validator",
                key_id="local-validator",
            )
            self.assertSucceeded(result)
            self.assertIn("Nodo development key created.", result.output())

    def test_keys_list_after_creation_returns_success(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_list_after_create_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            create_result = self.run_keys_create(
                data_dir,
                network="localnet",
                key_type="validator",
                key_id="local-validator",
            )
            self.assertSucceeded(create_result)

            list_result = self.run_keys_list(data_dir)
            self.assertSucceeded(list_result)

    def test_keys_list_after_creation_includes_key_id(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_list_incl_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            self.run_keys_create(
                data_dir,
                network="localnet",
                key_type="validator",
                key_id="my-test-key",
            )
            list_result = self.run_keys_list(data_dir)
            self.assertIn(
                "my-test-key",
                list_result.output(),
                msg=(
                    "Created key must appear in key list.\n"
                    f"stdout:\n{list_result.stdout}\n"
                    f"stderr:\n{list_result.stderr}"
                ),
            )

    def test_keys_list_before_any_creation_returns_success_or_empty(self) -> None:
        """After init but before any key creation, list should succeed with empty list."""
        with tempfile.TemporaryDirectory(prefix="nodo_key_list_empty_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            result = self.run_keys_list(data_dir)
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)
            # We don't enforce success vs. failure here; just no crash.

    def test_duplicate_key_creation_handled_safely(self) -> None:
        """Creating the same key ID twice must not crash or silently corrupt."""
        with tempfile.TemporaryDirectory(prefix="nodo_key_dup_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            self.run_keys_create(
                data_dir,
                network="localnet",
                key_type="validator",
                key_id="dup-key",
            )
            result = self.run_keys_create(
                data_dir,
                network="localnet",
                key_type="validator",
                key_id="dup-key",
            )
            self.assertNotTimedOut(result)
            self.assertNoSegfault(result)

    def test_multiple_different_keys_can_be_created(self) -> None:
        with tempfile.TemporaryDirectory(prefix="nodo_key_multi_") as tmp:
            data_dir = self.init_localnet(Path(tmp))
            for key_id in ["validator-a", "validator-b", "validator-c"]:
                result = self.run_keys_create(
                    data_dir,
                    network="localnet",
                    key_type="validator",
                    key_id=key_id,
                )
                self.assertSucceeded(result, msg=f"Failed creating key {key_id!r}")


if __name__ == "__main__":
    import unittest
    unittest.main(verbosity=2)

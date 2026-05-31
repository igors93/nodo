from __future__ import annotations

import unittest

from nodo_diag.source_audit import audit_sources
from nodo_diag.ctest_runner import find_repo_root


class SourcePersistenceAuditTests(unittest.TestCase):
    def test_finalized_store_and_loader_schema_ids_match(self) -> None:
        repo_root = find_repo_root()
        audit = audit_sources(repo_root)

        self.assertIsNotNone(audit.finalized_block_store_schema_id)
        self.assertIsNotNone(audit.runtime_state_loader_schema_id)
        self.assertEqual(
            audit.finalized_block_store_schema_id,
            audit.runtime_state_loader_schema_id,
            msg=(
                "FinalizedBlockStore and RuntimeStateLoader must agree on the "
                "finalized block schema id."
            ),
        )

    def test_persisted_fields_are_known_by_loader(self) -> None:
        repo_root = find_repo_root()
        audit = audit_sources(repo_root)

        self.assertEqual(
            [],
            audit.persisted_not_allowed,
            msg=(
                "FinalizedBlockStore persists fields that RuntimeStateLoader does not allow. "
                "Run diagnostics/python/run_failed_tests_diagnostics.py for the exact list."
            ),
        )


if __name__ == "__main__":
    unittest.main()

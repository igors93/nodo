from __future__ import annotations

import sys
import unittest
from pathlib import Path

_HERE = Path(__file__).resolve().parent
_PYTHON_ROOT = _HERE.parent
if str(_PYTHON_ROOT) not in sys.path:
    sys.path.insert(0, str(_PYTHON_ROOT))

from nodo_diag.ctest_runner import (
    DEFAULT_FOCUSED_TESTS,
    find_ctest_build_dir,
    find_repo_root,
    run_failed_ctests,
)


class FailingCTestDiagnostics(unittest.TestCase):
    def test_focused_ctests_generate_useful_output(self) -> None:
        repo_root = find_repo_root()
        build_dir = find_ctest_build_dir(repo_root)

        results = run_failed_ctests(build_dir, DEFAULT_FOCUSED_TESTS)

        self.assertEqual(len(results), len(DEFAULT_FOCUSED_TESTS))

        for result in results:
            combined_output = result.stdout + result.stderr
            self.assertTrue(
                combined_output.strip(),
                msg=f"No diagnostic output captured for: {' '.join(result.command)}",
            )


if __name__ == "__main__":
    unittest.main()

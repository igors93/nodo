from __future__ import annotations

import unittest

from nodo_diag.ctest_runner import (
    FAILED_TESTS,
    find_ctest_build_dir,
    find_repo_root,
    run_failed_ctests,
)


class FailingCTestDiagnostics(unittest.TestCase):
    def test_focused_ctests_generate_useful_output(self) -> None:
        repo_root = find_repo_root()
        build_dir = find_ctest_build_dir(repo_root)

        results = run_failed_ctests(build_dir, FAILED_TESTS)

        self.assertEqual(len(results), len(FAILED_TESTS))

        for result in results:
            combined_output = result.stdout + result.stderr
            self.assertTrue(
                combined_output.strip(),
                msg=f"No diagnostic output captured for: {' '.join(result.command)}",
            )


if __name__ == "__main__":
    unittest.main()

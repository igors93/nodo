from __future__ import annotations

"""
Unit tests for the Python helper modules in nodo_diag/.

These tests do NOT require the Nodo binary.  They test the Python logic
itself: file manipulation, assertion helpers, data generators, etc.

Categories:
  - filesystem_faults.py: read, write, remove_key, replace_key, append_key,
    break_order, require_file, manifest_path
  - cli_runner.py: NodoCliResult methods, find_repo_root
  - assertions.py: assert_succeeded, assert_failed, assert_failed_with_text,
    assert_succeeded_with_text, assert_clean_exit
  - generators.py: shape and content of generated tables
  - artifact_audit.py: parse_key_value_file
"""

import os
import sys
import tempfile
import unittest
from pathlib import Path

# ---------------------------------------------------------------------------
# Make nodo_diag importable regardless of CWD.
# ---------------------------------------------------------------------------
_HERE = Path(__file__).resolve().parent
_PYTHON_ROOT = _HERE.parent
if str(_PYTHON_ROOT) not in sys.path:
    sys.path.insert(0, str(_PYTHON_ROOT))

from nodo_diag.filesystem_faults import (
    append_key_value,
    break_file_canonical_order,
    manifest_path,
    read_text,
    remove_key_value_line,
    replace_key_value,
    require_file,
    write_text,
)
from nodo_diag.cli_runner import NodoCliResult, find_repo_root
from nodo_diag import assertions as asrt
from nodo_diag.generators import (
    bad_integer_values,
    bad_positive_integer_values,
    bad_hash_values,
    bad_config_id_values,
    bad_key_id_values,
    bad_key_types,
    bad_network_names,
    known_required_manifest_fields,
    official_networks,
    all_demo_commands,
    path_traversal_attempts,
    shell_metacharacter_values,
    unicode_payloads,
    very_long_values,
)
from nodo_diag.artifact_audit import parse_key_value_file


# ===========================================================================
# Helpers
# ===========================================================================

def _tmp_file(content: str, suffix: str = ".txt") -> tuple[tempfile.NamedTemporaryFile, Path]:
    f = tempfile.NamedTemporaryFile(mode="w", suffix=suffix, delete=False, encoding="utf-8")
    f.write(content)
    f.close()
    return Path(f.name)


def _make_result(returncode: int = 0, stdout: str = "", stderr: str = "") -> NodoCliResult:
    return NodoCliResult(
        command=["nodo", "status"],
        cwd="/tmp",
        returncode=returncode,
        duration_seconds=0.1,
        stdout=stdout,
        stderr=stderr,
    )


# ===========================================================================
# Tests: filesystem_faults.py
# ===========================================================================

class RequireFileTests(unittest.TestCase):

    def test_require_file_returns_path_when_file_exists(self) -> None:
        path = _tmp_file("hello")
        try:
            result = require_file(path)
            self.assertEqual(result, path)
        finally:
            path.unlink(missing_ok=True)

    def test_require_file_raises_when_file_missing(self) -> None:
        missing = Path("/tmp/nodo_test_nonexistent_file_999.txt")
        missing.unlink(missing_ok=True)
        with self.assertRaises(AssertionError):
            require_file(missing)

    def test_require_file_raises_when_path_is_directory(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(AssertionError):
                require_file(Path(tmp))


class ReadTextTests(unittest.TestCase):

    def test_read_text_returns_file_content(self) -> None:
        path = _tmp_file("hello world\n")
        try:
            self.assertEqual(read_text(path), "hello world\n")
        finally:
            path.unlink(missing_ok=True)

    def test_read_text_handles_utf8_with_errors(self) -> None:
        with tempfile.NamedTemporaryFile(delete=False, suffix=".txt") as f:
            f.write(b"valid\xff\xfeinvalid")
            tmp_path = Path(f.name)
        try:
            content = read_text(tmp_path)
            self.assertIn("valid", content)
        finally:
            tmp_path.unlink(missing_ok=True)

    def test_read_text_raises_when_file_missing(self) -> None:
        missing = Path("/tmp/nodo_test_no_read_file.txt")
        missing.unlink(missing_ok=True)
        with self.assertRaises(AssertionError):
            read_text(missing)


class WriteTextTests(unittest.TestCase):

    def test_write_text_creates_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "new_file.txt"
            write_text(path, "hello")
            self.assertTrue(path.is_file())
            self.assertEqual(path.read_text(encoding="utf-8"), "hello")

    def test_write_text_overwrites_existing(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "existing.txt"
            path.write_text("old content", encoding="utf-8")
            write_text(path, "new content")
            self.assertEqual(path.read_text(encoding="utf-8"), "new content")

    def test_write_text_creates_parent_directories(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "a" / "b" / "c" / "file.txt"
            write_text(path, "deep")
            self.assertTrue(path.is_file())

    def test_write_text_with_empty_content(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "empty.txt"
            write_text(path, "")
            self.assertEqual(path.read_text(encoding="utf-8"), "")

    def test_write_text_with_unicode_content(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "unicode.txt"
            write_text(path, "中文 emoji 🔥 arabic مرحبا")
            content = path.read_text(encoding="utf-8")
            self.assertIn("中文", content)


class RemoveKeyValueLineTests(unittest.TestCase):

    def test_removes_target_key(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "keyA=value1\nkeyB=value2\nkeyC=value3\n")
            remove_key_value_line(path, "keyB")
            content = read_text(path)
            self.assertNotIn("keyB", content)
            self.assertIn("keyA", content)
            self.assertIn("keyC", content)

    def test_raises_when_key_not_found(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "keyA=value1\n")
            with self.assertRaises(AssertionError):
                remove_key_value_line(path, "nonExistentKey")

    def test_removes_only_first_occurrence(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "keyA=v1\nkeyA=v2\nkeyB=v3\n")
            remove_key_value_line(path, "keyA")
            content = read_text(path)
            self.assertIn("keyB", content)

    def test_preserves_other_keys_exactly(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "schemaId=1\nchainId=abc\nlatestBlockHeight=0\n")
            remove_key_value_line(path, "chainId")
            content = read_text(path)
            self.assertIn("schemaId=1", content)
            self.assertIn("latestBlockHeight=0", content)
            self.assertNotIn("chainId", content)


class ReplaceKeyValueTests(unittest.TestCase):

    def test_replaces_value_for_existing_key(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "keyA=old\nkeyB=unchanged\n")
            replace_key_value(path, "keyA", "new")
            content = read_text(path)
            self.assertIn("keyA=new", content)
            self.assertIn("keyB=unchanged", content)

    def test_raises_when_key_not_found(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "keyA=value\n")
            with self.assertRaises(AssertionError):
                replace_key_value(path, "missingKey", "anything")

    def test_replaces_only_target_key(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "latestBlockHash=oldhash\nlatestStateRoot=oldroot\n")
            replace_key_value(path, "latestBlockHash", "newhash")
            content = read_text(path)
            self.assertIn("latestBlockHash=newhash", content)
            self.assertIn("latestStateRoot=oldroot", content)

    def test_replace_with_empty_value(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "keyA=value\n")
            replace_key_value(path, "keyA", "")
            content = read_text(path)
            self.assertIn("keyA=", content)


class AppendKeyValueTests(unittest.TestCase):

    def test_appends_new_key_value_line(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "keyA=v1\n")
            append_key_value(path, "newKey", "newValue")
            content = read_text(path)
            self.assertIn("newKey=newValue", content)
            self.assertIn("keyA=v1", content)

    def test_appends_to_file_without_trailing_newline(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "keyA=v1")  # no trailing newline
            append_key_value(path, "newKey", "v2")
            content = read_text(path)
            self.assertIn("keyA=v1", content)
            self.assertIn("newKey=v2", content)

    def test_appended_line_ends_with_newline(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "keyA=v1\n")
            append_key_value(path, "newKey", "v2")
            content = read_text(path)
            self.assertTrue(content.endswith("\n"))


class BreakFileCanonicalOrderTests(unittest.TestCase):

    def test_swaps_lines_1_and_2(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "line0\nline1\nline2\nline3\n")
            break_file_canonical_order(path)
            lines = read_text(path).splitlines()
            self.assertEqual(lines[0], "line0")
            self.assertEqual(lines[1], "line2")
            self.assertEqual(lines[2], "line1")

    def test_raises_when_file_too_small(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "a\nb\nc\n")  # only 3 lines
            with self.assertRaises(AssertionError):
                break_file_canonical_order(path)

    def test_file_with_exactly_4_lines_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.nodo"
            write_text(path, "a\nb\nc\nd\n")
            break_file_canonical_order(path)  # must not raise
            self.assertTrue(path.is_file())


class ManifestPathTests(unittest.TestCase):

    def test_manifest_path_returns_correct_subpath(self) -> None:
        data_dir = Path("/some/data/dir")
        expected = data_dir / "manifest.nodo"
        self.assertEqual(manifest_path(data_dir), expected)

    def test_manifest_path_has_correct_name(self) -> None:
        data_dir = Path("/any/path")
        self.assertEqual(manifest_path(data_dir).name, "manifest.nodo")


# ===========================================================================
# Tests: cli_runner.py — NodoCliResult
# ===========================================================================

class NodoCliResultTests(unittest.TestCase):

    def test_succeeded_returns_true_for_zero_returncode(self) -> None:
        r = _make_result(returncode=0)
        self.assertTrue(r.succeeded())

    def test_succeeded_returns_false_for_nonzero_returncode(self) -> None:
        r = _make_result(returncode=1)
        self.assertFalse(r.succeeded())

    def test_failed_returns_true_for_nonzero_returncode(self) -> None:
        r = _make_result(returncode=1)
        self.assertTrue(r.failed())

    def test_failed_returns_false_for_zero_returncode(self) -> None:
        r = _make_result(returncode=0)
        self.assertFalse(r.failed())

    def test_output_concatenates_stdout_and_stderr(self) -> None:
        r = _make_result(stdout="OUT\n", stderr="ERR\n")
        output = r.output()
        self.assertIn("OUT", output)
        self.assertIn("ERR", output)

    def test_output_with_empty_stderr(self) -> None:
        r = _make_result(stdout="ONLY STDOUT", stderr="")
        self.assertIn("ONLY STDOUT", r.output())

    def test_to_dict_contains_all_fields(self) -> None:
        r = _make_result(returncode=0, stdout="out", stderr="err")
        d = r.to_dict()
        self.assertIn("returncode", d)
        self.assertIn("stdout", d)
        self.assertIn("stderr", d)
        self.assertIn("command", d)
        self.assertIn("duration_seconds", d)


class FindRepoRootTests(unittest.TestCase):

    def test_find_repo_root_returns_path_with_cmakelists(self) -> None:
        # This test must be run from within the Nodo repo.
        try:
            root = find_repo_root()
            self.assertTrue((root / "CMakeLists.txt").is_file())
            self.assertTrue((root / "src").is_dir())
        except RuntimeError:
            self.skipTest("Not running inside the Nodo repository")

    def test_find_repo_root_raises_outside_repo(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            try:
                find_repo_root(Path(tmp))
                self.fail("Should have raised RuntimeError for non-repo directory")
            except RuntimeError:
                pass


# ===========================================================================
# Tests: assertions.py
# ===========================================================================

class AssertionsTests(unittest.TestCase):

    def test_assert_succeeded_passes_for_rc0(self) -> None:
        asrt.assert_succeeded(_make_result(returncode=0))

    def test_assert_succeeded_raises_for_rc1(self) -> None:
        with self.assertRaises(AssertionError):
            asrt.assert_succeeded(_make_result(returncode=1))

    def test_assert_failed_passes_for_rc1(self) -> None:
        asrt.assert_failed(_make_result(returncode=1))

    def test_assert_failed_raises_for_rc0(self) -> None:
        with self.assertRaises(AssertionError):
            asrt.assert_failed(_make_result(returncode=0))

    def test_assert_failed_with_text_passes_when_text_in_output(self) -> None:
        r = _make_result(returncode=1, stderr="expected text here")
        asrt.assert_failed_with_text(r, "expected text")

    def test_assert_failed_with_text_raises_when_text_missing(self) -> None:
        r = _make_result(returncode=1, stderr="something else")
        with self.assertRaises(AssertionError):
            asrt.assert_failed_with_text(r, "expected text")

    def test_assert_failed_with_text_raises_when_command_succeeded(self) -> None:
        r = _make_result(returncode=0, stderr="expected text")
        with self.assertRaises(AssertionError):
            asrt.assert_failed_with_text(r, "expected text")

    def test_assert_succeeded_with_text_passes(self) -> None:
        r = _make_result(returncode=0, stdout="hello world")
        asrt.assert_succeeded_with_text(r, "hello")

    def test_assert_succeeded_with_text_raises_when_text_missing(self) -> None:
        r = _make_result(returncode=0, stdout="something else")
        with self.assertRaises(AssertionError):
            asrt.assert_succeeded_with_text(r, "hello")

    def test_assert_succeeded_with_text_raises_when_command_failed(self) -> None:
        r = _make_result(returncode=1, stdout="hello")
        with self.assertRaises(AssertionError):
            asrt.assert_succeeded_with_text(r, "hello")

    def test_assert_not_timed_out_passes_for_normal_rc(self) -> None:
        for rc in [0, 1, 2, 127]:
            with self.subTest(rc=rc):
                asrt.assert_not_timed_out(_make_result(returncode=rc))

    def test_assert_not_timed_out_raises_for_rc_124(self) -> None:
        with self.assertRaises(AssertionError):
            asrt.assert_not_timed_out(_make_result(returncode=124))

    def test_assert_no_crash_passes_for_normal_codes(self) -> None:
        for rc in [0, 1, 2, 127]:
            with self.subTest(rc=rc):
                asrt.assert_no_crash(_make_result(returncode=rc))

    def test_assert_no_crash_raises_for_crash_codes(self) -> None:
        for rc in [134, 139, -11, -6]:
            with self.subTest(rc=rc):
                with self.assertRaises(AssertionError):
                    asrt.assert_no_crash(_make_result(returncode=rc))

    def test_assert_clean_exit_passes_for_rc_0(self) -> None:
        asrt.assert_clean_exit(_make_result(returncode=0))

    def test_assert_clean_exit_passes_for_rc_1(self) -> None:
        asrt.assert_clean_exit(_make_result(returncode=1))

    def test_assert_output_contains_all_passes(self) -> None:
        r = _make_result(returncode=0, stdout="alpha beta gamma")
        asrt.assert_output_contains_all(r, ["alpha", "beta", "gamma"])

    def test_assert_output_contains_all_raises_if_any_missing(self) -> None:
        r = _make_result(returncode=0, stdout="alpha beta")
        with self.assertRaises(AssertionError):
            asrt.assert_output_contains_all(r, ["alpha", "gamma"])

    def test_describe_result_returns_string(self) -> None:
        r = _make_result(returncode=0, stdout="hello", stderr="world")
        desc = asrt.describe_result(r)
        self.assertIsInstance(desc, str)
        self.assertIn("nodo", desc)


# ===========================================================================
# Tests: generators.py
# ===========================================================================

class GeneratorsTests(unittest.TestCase):

    def _check_list_of_tuples(self, lst, min_count: int = 2) -> None:
        self.assertIsInstance(lst, list)
        self.assertGreaterEqual(len(lst), min_count, msg=f"Expected at least {min_count} items")
        for item in lst:
            self.assertIsInstance(item, tuple)
            self.assertEqual(len(item), 2)
            label, value = item
            self.assertIsInstance(label, str)
            self.assertGreater(len(label), 0, msg="Label must not be empty")
            self.assertIsInstance(value, str)

    def test_bad_integer_values_has_expected_shape(self) -> None:
        self._check_list_of_tuples(bad_integer_values(), min_count=5)

    def test_bad_positive_integer_values_includes_zero(self) -> None:
        values = [v for _, v in bad_positive_integer_values()]
        self.assertIn("0", values, msg="bad_positive_integer_values must include '0'")

    def test_bad_hash_values_has_expected_shape(self) -> None:
        self._check_list_of_tuples(bad_hash_values(), min_count=5)

    def test_bad_config_id_values_has_expected_shape(self) -> None:
        self._check_list_of_tuples(bad_config_id_values(), min_count=3)

    def test_bad_key_id_values_has_expected_shape(self) -> None:
        self._check_list_of_tuples(bad_key_id_values(), min_count=5)

    def test_bad_key_types_has_expected_shape(self) -> None:
        self._check_list_of_tuples(bad_key_types(), min_count=3)

    def test_bad_network_names_has_expected_shape(self) -> None:
        self._check_list_of_tuples(bad_network_names(), min_count=3)

    def test_path_traversal_attempts_contains_parent_escape(self) -> None:
        values = [v for _, v in path_traversal_attempts()]
        has_escape = any(".." in v for v in values)
        self.assertTrue(has_escape, msg="path_traversal_attempts must contain '..' entries")

    def test_shell_metacharacter_values_has_expected_shape(self) -> None:
        self._check_list_of_tuples(shell_metacharacter_values(), min_count=5)

    def test_unicode_payloads_has_expected_shape(self) -> None:
        self._check_list_of_tuples(unicode_payloads(), min_count=3)

    def test_very_long_values_returns_correct_lengths(self) -> None:
        lengths = [128, 1024]
        result = very_long_values(lengths)
        self.assertEqual(len(result), 2)
        for (label, value), expected_len in zip(result, lengths):
            self.assertEqual(len(value), expected_len, msg=f"Length mismatch for {label}")

    def test_official_networks_returns_list_of_strings(self) -> None:
        nets = official_networks()
        self.assertIsInstance(nets, list)
        self.assertGreater(len(nets), 0)
        for n in nets:
            self.assertIsInstance(n, str)
            self.assertGreater(len(n), 0)

    def test_all_demo_commands_returns_expected_commands(self) -> None:
        cmds = all_demo_commands()
        self.assertIn("demo", cmds)
        self.assertIn("reload", cmds)
        self.assertGreaterEqual(len(cmds), 4)

    def test_known_required_manifest_fields_has_core_fields(self) -> None:
        fields = known_required_manifest_fields()
        self.assertIn("chainId", fields)
        self.assertIn("latestBlockHeight", fields)
        self.assertIn("validatorCount", fields)

    def test_labels_are_unique_within_each_generator(self) -> None:
        for fn_name, fn in [
            ("bad_integer_values", bad_integer_values),
            ("bad_hash_values", bad_hash_values),
            ("bad_key_id_values", bad_key_id_values),
        ]:
            with self.subTest(generator=fn_name):
                labels = [label for label, _ in fn()]
                self.assertEqual(
                    len(labels),
                    len(set(labels)),
                    msg=f"{fn_name}: duplicate label found in {labels}",
                )


# ===========================================================================
# Tests: artifact_audit.py — parse_key_value_file
# ===========================================================================

class ParseKeyValueFileTests(unittest.TestCase):

    def test_parses_simple_key_value_pairs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifact.kv"
            write_text(path, "key1=value1\nkey2=value2\n")
            result = parse_key_value_file(path)
            self.assertEqual(result["key1"], "value1")
            self.assertEqual(result["key2"], "value2")

    def test_ignores_comment_lines(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifact.kv"
            write_text(path, "# comment\nkey=value\n# another comment\n")
            result = parse_key_value_file(path)
            self.assertEqual(result, {"key": "value"})

    def test_ignores_blank_lines(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifact.kv"
            write_text(path, "\nkey=value\n\n")
            result = parse_key_value_file(path)
            self.assertEqual(result, {"key": "value"})

    def test_ignores_lines_without_equals(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifact.kv"
            write_text(path, "no-equals-here\nkey=value\n")
            result = parse_key_value_file(path)
            self.assertEqual(result, {"key": "value"})

    def test_splits_on_first_equals_only(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifact.kv"
            write_text(path, "key=val=ue=extra\n")
            result = parse_key_value_file(path)
            self.assertEqual(result["key"], "val=ue=extra")

    def test_returns_empty_dict_for_empty_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifact.kv"
            write_text(path, "")
            result = parse_key_value_file(path)
            self.assertEqual(result, {})

    def test_returns_empty_dict_for_comment_only_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifact.kv"
            write_text(path, "# comment only\n# another\n")
            result = parse_key_value_file(path)
            self.assertEqual(result, {})


if __name__ == "__main__":
    unittest.main(verbosity=2)

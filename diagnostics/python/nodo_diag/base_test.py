from __future__ import annotations

from pathlib import Path
import tempfile
import unittest

from nodo_diag.cli_runner import (
    NodoCliResult,
    find_nodo_binary,
    find_repo_root,
    run_nodo,
)
from nodo_diag.filesystem_faults import manifest_path


class NodoBaseTest(unittest.TestCase):
    """
    Shared base class for all Nodo scenario tests.

    Eliminates the setUp / helper duplication present in every scenario file.
    Subclasses get the binary check for free; they only need to call super().setUp()
    (or omit setUp entirely, since this class defines it).
    """

    def setUp(self) -> None:
        self.repo_root = find_repo_root()
        try:
            find_nodo_binary(self.repo_root)
        except RuntimeError as error:
            self.skipTest(str(error))

    # ------------------------------------------------------------------
    # Node lifecycle helpers
    # ------------------------------------------------------------------

    def init_localnet(self, temp_root: Path) -> Path:
        data_dir = temp_root / "node-data"
        result = run_nodo(
            ["init", "--network", "localnet", "--data-dir", str(data_dir)],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )
        self.assertEqual(
            result.returncode,
            0,
            msg=f"Nodo init failed:\n{result.output()}",
        )
        self.assertTrue(
            manifest_path(data_dir).is_file(),
            msg=f"Manifest not created after init: {manifest_path(data_dir)}",
        )
        return data_dir

    def run_status(self, data_dir: Path, network: str = "localnet") -> NodoCliResult:
        return run_nodo(
            ["status", "--network", network, "--data-dir", str(data_dir)],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )

    def run_reload(self, data_dir: Path, network: str = "localnet") -> NodoCliResult:
        return run_nodo(
            ["node", "reload", "--network", network, "--data-dir", str(data_dir)],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )

    def run_chain_audit(self, data_dir: Path, network: str = "localnet") -> NodoCliResult:
        return run_nodo(
            ["chain", "audit", "--network", network, "--data-dir", str(data_dir)],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )

    def run_keys_create(
        self,
        data_dir: Path,
        network: str = "localnet",
        key_type: str = "validator",
        key_id: str = "local-validator",
    ) -> NodoCliResult:
        return run_nodo(
            [
                "keys", "create",
                "--network", network,
                "--data-dir", str(data_dir),
                "--type", key_type,
                "--key-id", key_id,
            ],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )

    def run_keys_list(self, data_dir: Path) -> NodoCliResult:
        return run_nodo(
            ["keys", "list", "--data-dir", str(data_dir)],
            repo_root=self.repo_root,
            timeout_seconds=30,
        )

    def run_testnet_readiness(
        self,
        data_dir: Path,
        network: str = "testnet-candidate",
        key_id: str | None = None,
    ) -> NodoCliResult:
        args = ["testnet", "readiness", "--network", network, "--data-dir", str(data_dir)]
        if key_id is not None:
            args += ["--key-id", key_id]
        return run_nodo(args, repo_root=self.repo_root, timeout_seconds=30)

    def run_diagnostics(
        self,
        data_dir: Path,
        network: str = "testnet-candidate",
        key_id: str | None = None,
    ) -> NodoCliResult:
        args = ["diagnostics", "--network", network, "--data-dir", str(data_dir)]
        if key_id is not None:
            args += ["--key-id", key_id]
        return run_nodo(args, repo_root=self.repo_root, timeout_seconds=30)

    def temp_localnet(self) -> tuple[tempfile.TemporaryDirectory, Path]:
        """Returns (tmp_context_manager, data_dir). Caller must close the tmp."""
        tmp = tempfile.TemporaryDirectory(prefix="nodo_base_")
        data_dir = self.init_localnet(Path(tmp.name))
        return tmp, data_dir

    # ------------------------------------------------------------------
    # Assertion helpers
    # ------------------------------------------------------------------

    def assertSucceeded(self, result: NodoCliResult, msg: str = "") -> None:
        self.assertEqual(
            result.returncode,
            0,
            msg=(
                f"Expected command to succeed (rc=0) but got rc={result.returncode}.\n"
                f"{msg}\n"
                f"Command: {' '.join(result.command)}\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}"
            ),
        )

    def assertFailed(self, result: NodoCliResult, msg: str = "") -> None:
        self.assertNotEqual(
            result.returncode,
            0,
            msg=(
                f"Expected command to fail (rc!=0) but it succeeded.\n"
                f"{msg}\n"
                f"Command: {' '.join(result.command)}\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}"
            ),
        )

    def assertFailedWithText(
        self, result: NodoCliResult, text: str, msg: str = ""
    ) -> None:
        self.assertFailed(result, msg=msg)
        self.assertIn(
            text,
            result.output(),
            msg=(
                f"Expected output to contain {text!r}.\n"
                f"{msg}\n"
                f"Command: {' '.join(result.command)}\n"
                f"Return code: {result.returncode}\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}"
            ),
        )

    def assertSucceededWithText(
        self, result: NodoCliResult, text: str, msg: str = ""
    ) -> None:
        self.assertSucceeded(result, msg=msg)
        self.assertIn(
            text,
            result.output(),
            msg=(
                f"Expected output to contain {text!r}.\n"
                f"{msg}\n"
                f"Command: {' '.join(result.command)}\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}"
            ),
        )

    def assertNotTimedOut(self, result: NodoCliResult) -> None:
        self.assertNotEqual(
            result.returncode,
            124,
            msg=(
                "Command timed out (rc=124).\n"
                f"Command: {' '.join(result.command)}\n"
                f"stderr:\n{result.stderr}"
            ),
        )

    def assertNoSegfault(self, result: NodoCliResult) -> None:
        self.assertNotIn(
            result.returncode,
            {134, 139, -11, -6},
            msg=(
                f"Command crashed with signal (rc={result.returncode}).\n"
                f"Command: {' '.join(result.command)}\n"
                f"stderr:\n{result.stderr}"
            ),
        )

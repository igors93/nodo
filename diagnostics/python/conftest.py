"""
Pytest configuration: ensure the src/ package root is on sys.path so that
`nodo_diag` is importable in tests and scenarios without a prior `pip install`.
"""

from __future__ import annotations

import sys
from pathlib import Path

_SRC = Path(__file__).resolve().parent / "src"
if str(_SRC) not in sys.path:
    sys.path.insert(0, str(_SRC))

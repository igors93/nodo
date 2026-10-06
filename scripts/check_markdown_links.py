#!/usr/bin/env python3
"""Check local Markdown links without relying on a network service."""

from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r"!?\[[^\]]*\]\((?:<([^>]+)>|([^\s)]+))(?:\s+[^)]*)?\)")
INLINE_CODE = re.compile(r"`[^`]*`")
FENCED_CODE = re.compile(r"(?ms)^\s*(```|~~~).*?^\s*\1[^\n]*$")


def markdown_files():
    yield from (ROOT / "README.md", ROOT / "CONTRIBUTING.md", ROOT / "SECURITY.md")
    yield from (ROOT / "docs").rglob("*.md")
    yield from (ROOT / "diagnostics").rglob("*.md")


def main():
    errors = []
    for source in markdown_files():
        body = source.read_text(encoding="utf-8")
        body = FENCED_CODE.sub("", body)
        for line_no, line in enumerate(body.splitlines(), 1):
            line = INLINE_CODE.sub("", line)
            for match in LINK.finditer(line):
                target = match.group(1) or match.group(2)
                parsed = urlsplit(target)
                if parsed.scheme or parsed.netloc or target.startswith("//"):
                    continue
                if not parsed.path:
                    continue
                if parsed.path.startswith("/"):
                    errors.append(f"{source.relative_to(ROOT)}:{line_no}: absolute local link: {target}")
                    continue
                resolved = (source.parent / unquote(parsed.path)).resolve()
                if not resolved.is_relative_to(ROOT) or not resolved.exists():
                    errors.append(f"{source.relative_to(ROOT)}:{line_no}: missing: {target}")
    for error in errors:
        print(error, file=sys.stderr)
    print(f"Checked local Markdown links: {len(errors)} error(s)")
    return bool(errors)


if __name__ == "__main__":
    sys.exit(main())

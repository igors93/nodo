from __future__ import annotations

from pathlib import Path


def require_file(path: Path) -> Path:
    if not path.is_file():
        raise AssertionError(f"Expected file does not exist: {path}")
    return path


def read_text(path: Path) -> str:
    require_file(path)
    return path.read_text(encoding="utf-8", errors="replace")


def write_text(path: Path, contents: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8")


def manifest_path(data_dir: Path) -> Path:
    return data_dir / "manifest.nodo"


def delete_file(path: Path) -> None:
    require_file(path)
    path.unlink()


def remove_key_value_line(path: Path, key: str) -> None:
    text = read_text(path)
    lines = text.splitlines()

    prefix = f"{key}="
    new_lines = [line for line in lines if not line.startswith(prefix)]

    if len(new_lines) == len(lines):
        raise AssertionError(f"Key not found in file {path}: {key}")

    write_text(path, "\n".join(new_lines) + "\n")


def replace_key_value(path: Path, key: str, new_value: str) -> None:
    text = read_text(path)
    lines = text.splitlines()

    prefix = f"{key}="
    replaced = False
    new_lines: list[str] = []

    for line in lines:
        if line.startswith(prefix):
            new_lines.append(f"{key}={new_value}")
            replaced = True
        else:
            new_lines.append(line)

    if not replaced:
        raise AssertionError(f"Key not found in file {path}: {key}")

    write_text(path, "\n".join(new_lines) + "\n")


def append_key_value(path: Path, key: str, value: str) -> None:
    text = read_text(path)

    if text and not text.endswith("\n"):
        text += "\n"

    text += f"{key}={value}\n"

    write_text(path, text)


def break_file_canonical_order(path: Path) -> None:
    text = read_text(path)
    lines = text.splitlines()

    if len(lines) < 4:
        raise AssertionError(f"File is too small to reorder safely: {path}")

    # Keep the schema/header line in place and swap two field lines.
    lines[1], lines[2] = lines[2], lines[1]

    write_text(path, "\n".join(lines) + "\n")
from __future__ import annotations

"""
Test data generators for Nodo diagnostic scenarios.

Each function returns a list of (label, value) tuples suitable for use
with unittest.subTest(label=label, value=value).

Convention:
    label  -- short identifier used in failure messages (no spaces)
    value  -- the actual string to inject into the manifest or CLI arg
"""


# ---------------------------------------------------------------------------
# Numeric field corruption
# ---------------------------------------------------------------------------

def bad_integer_values() -> list[tuple[str, str]]:
    """Values that are not valid non-negative integers."""
    return [
        ("empty",           ""),
        ("space",           " "),
        ("not_a_number",    "not-a-number"),
        ("float",           "1.5"),
        ("scientific",      "1e10"),
        ("negative",        "-1"),
        ("negative_zero",   "-0"),
        ("nan",             "NaN"),
        ("inf",             "Inf"),
        ("neg_inf",         "-Inf"),
        ("infinity_word",   "infinity"),
        ("hex",             "0x10"),
        ("leading_space",   " 1"),
        ("trailing_space",  "1 "),
        ("two_numbers",     "1 2"),
        ("very_large",      "9" * 20),
        ("null_word",       "null"),
        ("none_word",       "None"),
        ("true_word",       "true"),
        ("false_word",      "false"),
        ("plus_prefix",     "+1"),
        ("newline_in",      "1\n2"),
    ]


def bad_positive_integer_values() -> list[tuple[str, str]]:
    """Values that are not valid strictly-positive integers (e.g. validatorCount)."""
    base = bad_integer_values()
    return base + [
        ("zero",  "0"),
    ]


def bad_hash_values() -> list[tuple[str, str]]:
    """Values that should be rejected as cryptographic hash/root fields."""
    return [
        ("empty",           ""),
        ("space",           " "),
        ("plain_word",      "not-a-hash"),
        ("hex_prefix",      "0x" + "a" * 64),
        ("non_hex_chars",   "g" * 64),
        ("too_short",       "a" * 63),
        ("too_long",        "a" * 65),
        ("very_long",       "a" * 256),
        ("null_word",       "null"),
        ("none_word",       "None"),
        ("newline",         "\n"),
        ("tab",             "\t"),
        ("upper_invalid",   "ZZZZZZZZ" * 8),
        ("all_zeros",       "0" * 64),
        ("mixed_garbage",   "bad-python-diagnostic-hash-value-injected-by-test"),
    ]


def bad_config_id_values() -> list[tuple[str, str]]:
    """Values that should be rejected as genesisConfigId or similar identifier fields."""
    return [
        ("empty",           ""),
        ("space",           " "),
        ("random_string",   "bad-python-diagnostic-genesis-id"),
        ("very_long",       "x" * 512),
        ("null_word",       "null"),
        ("json_fragment",   '{"key": "value"}'),
        ("path_like",       "/etc/passwd"),
        ("newline",         "line1\nline2"),
    ]


# ---------------------------------------------------------------------------
# String field corruption (generic identifiers)
# ---------------------------------------------------------------------------

def bad_network_names() -> list[tuple[str, str]]:
    """Network names that should be rejected by the CLI."""
    return [
        ("empty",           ""),
        ("space",           " "),
        ("unknown",         "unknown-network"),
        ("typo_testnet",    "testnet"),
        ("typo_mainnet",    "mainnett"),
        ("numeric",         "12345"),
        ("path_traversal",  "../../etc"),
        ("semicolon",       "localnet;id"),
        ("null",            "null"),
        ("very_long",       "n" * 256),
    ]


def bad_key_id_values() -> list[tuple[str, str]]:
    """Key IDs that should be rejected or handled safely."""
    return [
        ("empty",           ""),
        ("space_only",      " "),
        ("path_traversal",  "../../etc/passwd"),
        ("abs_path",        "/etc/passwd"),
        ("windows_path",    "C:\\Windows\\System32"),
        ("null_byte",       "key\x00name"),
        ("semicolon",       "key;id"),
        ("ampersand",       "key&id"),
        ("pipe",            "key|id"),
        ("newline",         "key\nid"),
        ("very_long",       "k" * 512),
        ("dot_dot",         ".."),
        ("single_dot",      "."),
        ("slash",           "key/subkey"),
        ("backslash",       "key\\subkey"),
    ]


def bad_key_types() -> list[tuple[str, str]]:
    """Key types that should be rejected."""
    return [
        ("empty",           ""),
        ("unknown",         "unknown-type"),
        ("admin",           "admin"),
        ("root",            "root"),
        ("numeric",         "42"),
        ("path_traversal",  "../../etc"),
        ("very_long",       "t" * 128),
    ]


# ---------------------------------------------------------------------------
# Path / data-dir edge cases
# ---------------------------------------------------------------------------

def path_traversal_attempts() -> list[tuple[str, str]]:
    """Paths that attempt to escape the intended directory."""
    return [
        ("parent_escape",       "../../etc/passwd"),
        ("many_parents",        "../" * 20 + "etc/passwd"),
        ("abs_unix",            "/etc/passwd"),
        ("abs_windows",         "C:\\Windows\\System32\\cmd.exe"),
        ("null_byte",           "/tmp/safe\x00/etc/passwd"),
        ("url_encoded",         "%2e%2e%2fetc%2fpasswd"),
        ("mixed_slashes",       "..\\../etc/passwd"),
        ("tilde_home",          "~/../../etc/passwd"),
    ]


# ---------------------------------------------------------------------------
# Known official networks
# ---------------------------------------------------------------------------

def official_networks() -> list[str]:
    """Networks that enforce restrictions (demo blocked, key-id required, etc.)."""
    return ["testnet-candidate"]


def all_demo_commands() -> list[str]:
    """CLI subcommands that are blocked on official networks."""
    return [
        "demo",
    ]


# ---------------------------------------------------------------------------
# Manifest required fields
# ---------------------------------------------------------------------------

def known_required_manifest_fields() -> list[str]:
    """
    Manifest fields confirmed by existing test coverage.
    Removing any one of these must cause 'status' to fail.

    NOTE: 'selectedNetwork' is intentionally absent — the manifest does not
    store it under that name; network identity is derived from chainId/genesisConfigId.
    """
    return [
        "chainId",
        "latestBlockHeight",
        "validatorCount",
        "latestBlockHash",
        "genesisConfigId",
        "latestStateRoot",
    ]


def known_hash_manifest_fields() -> list[str]:
    """Manifest fields that hold cryptographic hashes / roots."""
    return [
        "latestBlockHash",
        "latestStateRoot",
        "genesisConfigId",
    ]


def known_numeric_manifest_fields() -> list[str]:
    """Manifest fields that hold integer values."""
    return [
        "latestBlockHeight",
    ]


def known_positive_numeric_manifest_fields() -> list[str]:
    """Manifest fields that hold strictly-positive integer values."""
    return [
        "validatorCount",
    ]


# ---------------------------------------------------------------------------
# Unknown field names (should be rejected by strict parsers)
# ---------------------------------------------------------------------------

def unknown_field_names() -> list[tuple[str, str]]:
    """Unknown field names to inject. Each causes a strict-parse failure."""
    return [
        ("python_diag_field",   "unexpectedPythonDiagnosticField"),
        ("camel_case",          "someRandomCamelCaseField"),
        ("empty_name",          ""),
        ("numeric_name",        "12345"),
        ("spaces_in_name",      "field name"),
        ("dot_prefix",          ".hidden"),
        ("double_underscore",   "__proto__"),
        ("sql_injection",       "field'; DROP TABLE blocks;--"),
    ]


# ---------------------------------------------------------------------------
# Security payloads
# ---------------------------------------------------------------------------

def shell_metacharacter_values() -> list[tuple[str, str]]:
    """Values containing shell metacharacters.

    Because subprocess is invoked in list-form (not shell=True), these are
    passed literally to the binary -- no shell expansion occurs in Python.
    The tests verify the binary itself does not interpret them.
    """
    return [
        ("semicolon",       "value;id"),
        ("pipe",            "value|cat /etc/passwd"),
        ("ampersand",       "value&id"),
        ("backtick",        "value`id`"),
        ("dollar_sub",      "value$(id)"),
        ("redirect_out",    "value>/tmp/x"),
        ("redirect_in",     "value</etc/passwd"),
        ("double_amp",      "value&&id"),
        ("double_pipe",     "value||id"),
        ("newline_cmd",     "value\nid\n"),
    ]


def unicode_payloads() -> list[tuple[str, str]]:
    """Unicode values that should be handled safely (not crash the binary).

    NOTE: literal null bytes and lone surrogates are expressed via Python
    escape sequences to keep the source file clean of control characters.
    """
    return [
        ("emoji",           "\U0001f4a3"),
        ("arabic",          "مرحبا"),   # مرحبا
        ("cjk",             "中文"),                      # 中文
        ("rtl_override",    "‮"),
        ("null_byte",       "\x00"),
        ("bom",             "﻿"),
        ("replacement",     "�"),
        ("lone_surrogate",  "\ud800"),
    ]


def very_long_values(lengths: list[int] | None = None) -> list[tuple[str, str]]:
    """Values of extreme length (should not crash or hang the binary).

    NOTE: Windows CreateProcess has a total command-line limit of ~32767 characters,
    so the default cap is 4096 to stay safely within that budget.  Callers that
    want to test OS-level rejection should use lengths <= 4096.
    """
    if lengths is None:
        lengths = [256, 1024, 4096]
    return [(f"len_{n}", "A" * n) for n in lengths]

#!/usr/bin/env python3
"""Compare clang-tidy binaries to find checks and options added between
versions.

Given two or more binaries, the script auto-detects their major versions
(using ``clang-tidy --version``), sorts them newest-first, then emits the
diff for each adjacent pair: ``N vs N-1``, ``N-1 vs N-2``, ...

Usage::

    python3 compare_clang_tidy_versions_multi.py \\
        /path/to/clang-tidy-22 \\
        /path/to/clang-tidy-21 \\
        /path/to/clang-tidy-20 \\
        /path/to/clang-tidy-19

Two binaries produce a single "new vs old" diff:

    python3 compare_clang_tidy_versions_multi.py \\
        --old /path/to/clang-tidy-18 --new /path/to/clang-tidy-19

To obtain binaries for past releases:
  https://github.com/llvm/llvm-project/releases  (pre-built tarballs)
  or build from a git tag:  git checkout llvmorg-18.1.0 && ninja clang-tidy

Output is a YAML stream of documents (one per pair), separated by ``---``.
"""

import argparse
import re
import subprocess
import sys
from typing import Dict, Set


def _run(binary: str, args: list[str]) -> str:
    try:
        result = subprocess.run(
            [binary] + args,
            capture_output=True,
            text=True,
            timeout=120,
        )
    except FileNotFoundError:
        sys.exit(f"error: binary not found: {binary}")
    except subprocess.TimeoutExpired:
        sys.exit(f"error: timed out running {binary}")
    return result.stdout


def get_version(binary: str) -> str:
    output = _run(binary, ["--version"])
    m = re.search(r"version\s+(\d+)(?:\.\d+){0,2}", output, re.IGNORECASE)
    if not m:
        sys.exit(f"error: could not detect version from {binary} --version")
    return m.group(1)


def get_checks(binary: str) -> Set[str]:
    output = _run(binary, ["--list-checks", "-checks=*"])
    checks: Set[str] = set()
    for line in output.splitlines():
        stripped = line.strip()
        # Skip the header line and empty lines
        if not stripped or stripped.startswith("Enabled checks:"):
            continue
        checks.add(stripped)
    return checks


def _parse_check_options(yaml_text: str) -> Dict[str, Set[str]]:
    """Parse ``CheckOptions`` from clang-tidy ``--dump-config`` output.

    clang-tidy has emitted two YAML shapes across versions:

    Modern (mapping):
        CheckOptions:
          bugprone-argument-comment.StrictMode: 'false'

    Older (list-of-dicts):
        CheckOptions:
          - key: bugprone-argument-comment.StrictMode
            value: 'false'

    We handle both with simple regex rather than a full YAML parser so the
    script stays dependency-free.
    """
    options: Dict[str, Set[str]] = {}

    # Find the CheckOptions block
    check_options_match = re.search(
        r"CheckOptions\s*:(.*?)(?=\n\S|\Z)", yaml_text, re.DOTALL
    )
    if not check_options_match:
        return options

    block = check_options_match.group(1)

    # Modern format: "  CheckName.OptionName: 'value'"
    for m in re.finditer(r"^\s+([\w-]+)\.([\w-]+)\s*:", block, re.MULTILINE):
        check, opt = m.group(1), m.group(2)
        options.setdefault(check, set()).add(opt)

    # Older format: "  - key: CheckName.OptionName"
    for m in re.finditer(r"key\s*:\s*([\w-]+)\.([\w-]+)", block):
        check, opt = m.group(1), m.group(2)
        options.setdefault(check, set()).add(opt)

    return options


def get_options(binary: str) -> Dict[str, Set[str]]:
    """Return ``{check_name: {option_name, ...}}`` from ``--dump-config``."""
    output = _run(binary, ["--dump-config", "-checks=*"])
    return _parse_check_options(output)


def _yaml_list(items: list[str], indent: str = "  ") -> str:
    if not items:
        return " []"
    return "\n" + "\n".join(f"{indent}- {i}" for i in sorted(items))


def yaml_report(
    old_ver: str,
    new_ver: str,
    added_checks: Set[str],
    removed_checks: Set[str],
    added_options: Dict[str, Set[str]],
    removed_options: Dict[str, Set[str]],
) -> str:
    added_opt_flat = sorted(f"{c}.{o}" for c, opts in added_options.items() for o in opts)
    removed_opt_flat = sorted(f"{c}.{o}" for c, opts in removed_options.items() for o in opts)
    lines = [
        f"new_version: '{new_ver}'",
        f"old_version: '{old_ver}'",
        f"added_checks:{_yaml_list(list(added_checks))}",
        f"removed_checks:{_yaml_list(list(removed_checks))}",
        f"added_options:{_yaml_list(added_opt_flat)}",
        f"removed_options:{_yaml_list(removed_opt_flat)}",
    ]
    return "\n".join(lines)


def baseline_doc(binary: str) -> str:
    """Emit a YAML doc describing the full inventory of the oldest binary.

    Checks/options present in the oldest binary never show up in any pairwise
    ``added_*`` set (there is nothing older to diff against), so a consumer that
    wants to treat them as "present since <oldest>" needs this baseline.
    """
    version = get_version(binary)
    checks = get_checks(binary)
    options = get_options(binary)
    opt_flat = sorted(f"{c}.{o}" for c, opts in options.items() for o in opts)
    lines = [
        f"baseline_version: '{version}'",
        f"baseline_checks:{_yaml_list(sorted(checks))}",
        f"baseline_options:{_yaml_list(opt_flat)}",
    ]
    return "\n".join(lines)


def diff_pair(new_binary: str, old_binary: str) -> str:
    new_ver = get_version(new_binary)
    old_ver = get_version(old_binary)

    new_checks = get_checks(new_binary)
    old_checks = get_checks(old_binary)
    new_opts = get_options(new_binary)
    old_opts = get_options(old_binary)

    added_checks = new_checks - old_checks
    removed_checks = old_checks - new_checks

    added_options: Dict[str, Set[str]] = {}
    removed_options: Dict[str, Set[str]] = {}
    for check in new_opts.keys() & old_opts.keys():
        added = new_opts[check] - old_opts[check]
        removed = old_opts[check] - new_opts[check]
        if added:
            added_options[check] = added
        if removed:
            removed_options[check] = removed

    return yaml_report(
        old_ver,
        new_ver,
        added_checks,
        removed_checks,
        added_options,
        removed_options,
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "binaries",
        nargs="*",
        metavar="BINARY",
        help="paths to clang-tidy binaries (any order); need at least two, "
             "either positionally or via --old/--new",
    )
    parser.add_argument("--old", metavar="BINARY",
                        help="path to the older clang-tidy binary "
                             "(shorthand for a two-binary comparison)")
    parser.add_argument("--new", metavar="BINARY",
                        help="path to the newer clang-tidy binary "
                             "(shorthand for a two-binary comparison)")
    parser.add_argument(
        "--with-baseline",
        action="store_true",
        help="prepend a YAML doc listing the full inventory of the oldest "
             "binary (needed to treat its checks/options as 'present since "
             "<oldest>' when backfilling doc versions)",
    )
    args = parser.parse_args()

    if bool(args.old) != bool(args.new):
        parser.error("--old and --new must be given together")
    binaries = args.binaries + ([args.old, args.new] if args.old else [])

    if len(binaries) < 2:
        parser.error("need at least two binaries to compare")

    # Pair each binary with its detected major version, sort newest first.
    versioned = [(int(get_version(b)), b) for b in binaries]
    versioned.sort(key=lambda x: x[0], reverse=True)

    docs = []
    if args.with_baseline:
        oldest_bin = versioned[-1][1]
        docs.append("# === baseline ===\n" + baseline_doc(oldest_bin))
    for (new_ver, new_bin), (old_ver, old_bin) in zip(versioned, versioned[1:]):
        header = f"# === {new_ver} vs {old_ver} ==="
        docs.append(header + "\n" + diff_pair(new_bin, old_bin))

    print("\n---\n".join(docs))


if __name__ == "__main__":
    main()

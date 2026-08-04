#!/usr/bin/env python3
"""Backfill ``.. versionadded::`` directives into clang-tidy check docs.

Consumes the YAML stream produced by ``compare_clang_tidy_versions_multi.py``
and, for every check / check option, determines the clang-tidy major version in
which it first appeared, then edits the corresponding ``.rst`` file in
``clang-tools-extra/docs/clang-tidy/checks/`` to insert a Sphinx
``.. versionadded::`` directive.

  * A check appearing in ``added_checks`` of the "N vs N-1" diff was introduced
    in version N -> ``.. versionadded:: N`` after the doc title.
  * An option in ``added_options`` similarly -> ``.. versionadded:: N`` indented
    under its ``.. option::`` block.

Checks/options that are already present in the oldest binary never show up in
any ``added_*`` set. They are handled per ``--floor``:

    --floor skip      (default) leave them un-annotated; their true origin is
                      unknown (could predate the compared range).
    --floor annotate  label them ``.. versionadded:: <baseline>`` using the
                      baseline doc (requires the YAML to have been produced with
                      ``--with-baseline``).

The script is idempotent: a block that already carries a ``versionadded`` is
left untouched.

Usage::

    python3 compare_clang_tidy_versions_multi.py --with-baseline \\
        ~/clang-tidy-builds/bin/clang-tidy-* /usr/bin/clang-tidy-2? \\
        > versions.yaml

    python3 backfill_doc_versions.py versions.yaml          # writes docs
    python3 backfill_doc_versions.py versions.yaml --dry-run # preview only
"""

import argparse
import os
import re
import sys
from typing import Dict, List, Optional, Tuple

# Repo-relative path to the check docs, resolved from this file's location.
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
DOCS_CHECKS_DIR = os.path.normpath(
    os.path.join(_THIS_DIR, "..", "..", "docs", "clang-tidy", "checks")
)

# clang-analyzer docs are auto-generated redirect stubs; never edit them.
SKIP_CATEGORIES = {"clang-analyzer"}


# --------------------------------------------------------------------------- #
# Minimal YAML-stream parsing (matches the simple shape our tools emit).
# --------------------------------------------------------------------------- #

def parse_documents(text: str) -> List[Dict[str, object]]:
    """Split a ``---``-separated stream into dicts of scalars and string lists.

    Only the flat ``key: value`` / ``key:`` + ``- item`` shapes emitted by the
    comparison tools are supported (no nesting), so we avoid a YAML dependency.
    """
    docs: List[Dict[str, object]] = []
    for chunk in re.split(r"^---\s*$", text, flags=re.MULTILINE):
        doc: Dict[str, object] = {}
        current_key: Optional[str] = None
        for raw in chunk.splitlines():
            line = raw.rstrip()
            if not line or line.lstrip().startswith("#"):
                continue
            item = re.match(r"\s*-\s+(.*)$", line)
            if item and current_key is not None:
                doc.setdefault(current_key, [])
                assert isinstance(doc[current_key], list)
                doc[current_key].append(item.group(1).strip())  # type: ignore[union-attr]
                continue
            kv = re.match(r"([\w-]+)\s*:\s*(.*)$", line)
            if kv:
                key, val = kv.group(1), kv.group(2).strip()
                current_key = key
                if val == "[]":
                    doc[key] = []
                elif val == "":
                    # Key whose list items follow on subsequent lines; leave
                    # unset so the "- item" branch can setdefault it to a list.
                    pass
                else:
                    doc[key] = val.strip("'\"")
        if doc:
            docs.append(doc)
    return docs


# --------------------------------------------------------------------------- #
# Build {check: version} and {check.option: version} maps from the stream.
# --------------------------------------------------------------------------- #

def build_version_maps(
    docs: List[Dict[str, object]], floor: str
) -> Tuple[Dict[str, str], Dict[str, str]]:
    check_ver: Dict[str, str] = {}
    option_ver: Dict[str, str] = {}

    def record(mapping: Dict[str, str], key: str, version: str) -> None:
        # Keep the lowest version if a name somehow appears more than once
        # (e.g. removed then re-added).
        if key not in mapping or int(version) < int(mapping[key]):
            mapping[key] = version

    baseline: Optional[Dict[str, object]] = None
    for doc in docs:
        if "baseline_version" in doc:
            baseline = doc
            continue
        version = doc.get("new_version")
        if not isinstance(version, str):
            continue
        for c in doc.get("added_checks") or []:
            record(check_ver, c, version)
        for o in doc.get("added_options") or []:
            record(option_ver, o, version)

    if floor == "annotate":
        if baseline is None:
            sys.exit(
                "error: --floor annotate requires a baseline doc; rerun the "
                "comparison with --with-baseline"
            )
        bver = baseline["baseline_version"]
        assert isinstance(bver, str)
        for c in baseline.get("baseline_checks") or []:
            record(check_ver, c, bver)
        for o in baseline.get("baseline_options") or []:
            record(option_ver, o, bver)

    return check_ver, option_ver


# --------------------------------------------------------------------------- #
# Map a check name to its .rst path using the real category directories.
# --------------------------------------------------------------------------- #

def _category_dirs() -> List[str]:
    return sorted(
        (d for d in os.listdir(DOCS_CHECKS_DIR)
         if os.path.isdir(os.path.join(DOCS_CHECKS_DIR, d))),
        key=len,
        reverse=True,  # longest first so "clang-analyzer" wins over "clang"
    )


def check_to_path(
    check: str, categories: List[str]
) -> Tuple[Optional[str], Optional[str]]:
    for cat in categories:
        prefix = cat + "-"
        if check.startswith(prefix):
            rel = check[len(prefix):]
            return os.path.join(DOCS_CHECKS_DIR, cat, rel + ".rst"), cat
    return None, None


# --------------------------------------------------------------------------- #
# Editing primitives.
#
# Both check and option annotations render as a clang-format-style inline badge,
# handled by the local versionbadge Sphinx extension
# (clang-tools-extra/docs/_ext/versionbadge.py):
#   * Check title -> a ".. check-version:: N" directive just below the title;
#     the extension hoists it into the H1 as an inline badge after the nav/TOC
#     title has been captured (so the version does not leak into navigation).
#   * Option -> a ":versionadded: N" field on the ".. option::" directive,
#     rendered inline next to the option name.
# The ".versionbadge" CSS is defined via rst_prolog in
# clang-tools-extra/docs/conf.py.
# --------------------------------------------------------------------------- #

# Already-present check-title badge directive (idempotency guard).
CHECK_BADGE_RE = re.compile(r"^\s*\.\.\s+check-version::")
# Already-present option ":versionadded:" field (idempotency guard).
OPTION_FIELD_RE = re.compile(r"^\s*:versionadded:\s")


def insert_check_versionadded(lines: List[str], version: str) -> bool:
    """Insert a ``.. check-version::`` directive after the title underline."""
    if any(CHECK_BADGE_RE.match(l) for l in lines):
        return False  # already annotated; be conservative
    # Find the title: a line followed by an all-``=`` underline of >= its length.
    for i in range(len(lines) - 1):
        title, under = lines[i].strip(), lines[i + 1].strip()
        if (title and under and set(under) == {"="}
                and len(under) >= len(title)):
            block = ["", f".. check-version:: {version}"]
            # Insert a blank line + directive right after the underline.
            lines[i + 2:i + 2] = block
            return True
    return False


def insert_option_versionadded(
    lines: List[str], option: str, version: str
) -> bool:
    """Add a ``:versionadded:`` field to the ``.. option:: <option>`` block.

    Directive fields must immediately follow the directive line (before the
    blank line that precedes the body), so the field is inserted right after
    the ``.. option::`` line.
    """
    pat = re.compile(rf"^(\s*)\.\.\s+option::\s+{re.escape(option)}\s*$")
    for i, line in enumerate(lines):
        m = pat.match(line)
        if not m:
            continue
        field_indent = m.group(1) + "   "  # directive content indent (3 spaces)
        # If the directive already has a versionadded field, leave it alone.
        if i + 1 < len(lines) and OPTION_FIELD_RE.match(lines[i + 1]):
            return False
        lines[i + 1:i + 1] = [f"{field_indent}:versionadded: {version}"]
        return True
    return False


# --------------------------------------------------------------------------- #
# Main.
# --------------------------------------------------------------------------- #

def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("yaml", metavar="YAML",
                        help="YAML stream from compare_clang_tidy_versions_multi.py "
                             "(use '-' for stdin)")
    parser.add_argument("--floor", choices=["skip", "annotate"], default="skip",
                        help="how to treat checks/options present since the "
                             "oldest compared binary (default: skip)")
    parser.add_argument("--dry-run", action="store_true",
                        help="show what would change without writing files")
    args = parser.parse_args()

    text = sys.stdin.read() if args.yaml == "-" else open(args.yaml).read()
    docs = parse_documents(text)
    check_ver, option_ver = build_version_maps(docs, args.floor)

    if not check_ver and not option_ver:
        sys.exit("error: no version information parsed from input")

    categories = _category_dirs()

    # Group option versions by their owning check for one edit pass per file.
    opts_by_check: Dict[str, Dict[str, str]] = {}
    for full, ver in option_ver.items():
        check, _, opt = full.partition(".")
        opts_by_check.setdefault(check, {})[opt] = ver

    all_checks = set(check_ver) | set(opts_by_check)
    edited = skipped = missing = 0

    for check in sorted(all_checks):
        path, cat = check_to_path(check, categories)
        if path is None:
            print(f"  ?  no category dir for {check}", file=sys.stderr)
            missing += 1
            continue
        if cat in SKIP_CATEGORIES:
            continue
        if not os.path.isfile(path):
            print(f"  ?  no doc file for {check} ({path})", file=sys.stderr)
            missing += 1
            continue

        with open(path) as f:
            lines = f.read().splitlines()
        changed = False

        if check in check_ver:
            if insert_check_versionadded(lines, check_ver[check]):
                changed = True

        for opt, ver in sorted(opts_by_check.get(check, {}).items()):
            if insert_option_versionadded(lines, opt, ver):
                changed = True

        rel = os.path.relpath(path, DOCS_CHECKS_DIR)
        if changed:
            edited += 1
            if args.dry_run:
                print(f"  + would edit {rel}")
            else:
                with open(path, "w") as f:
                    f.write("\n".join(lines) + "\n")
                print(f"  + edited {rel}")
        else:
            skipped += 1

    verb = "would edit" if args.dry_run else "edited"
    print(f"\n{verb} {edited} file(s); {skipped} unchanged; {missing} missing.",
          file=sys.stderr)


if __name__ == "__main__":
    main()

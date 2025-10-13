#!/usr/bin/env python3
#
# ===-----------------------------------------------------------------------===#
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# ===-----------------------------------------------------------------------===#

"""Script to add a new clang-tidy check alias."""

import argparse
import io
import os
import re
import sys


def get_camel_name(check_name: str) -> str:
    """Convert dash-separated name to CamelCase."""
    return "".join(elem.capitalize() for elem in check_name.split("-"))


def get_camel_check_name(check_name: str) -> str:
    """Convert dash-separated name to CamelCase with 'Check' suffix."""
    return get_camel_name(check_name) + "Check"


def get_module_filename(module_path: str, module: str) -> str:
    """Return the source filename that implements the module."""
    module_files = [
        f
        for f in os.listdir(module_path)
        if f.lower() == f"{module.lower()}tidymodule.cpp"
    ]
    return os.path.join(module_path, module_files[0])


def register_alias_in_module(
    alias_module_path: str,
    alias_module: str,
    alias_check: str,
    current_module: str,
    current_check_class: str,
) -> None:
    """Add the alias registration to the module."""
    filename = get_module_filename(alias_module_path, alias_module)
    with io.open(filename, "r", encoding="utf8") as f:
        lines = f.readlines()

    alias_fq_name = f"{alias_module}-{alias_check}"

    # Determine the include line
    if alias_module != current_module:
        include_line = f'#include "../{current_module}/{current_check_class}.h"\n'
        namespace_prefix = f"{current_module}::"
    else:
        include_line = f'#include "{current_check_class}.h"\n'
        namespace_prefix = ""

    # Create the alias declaration
    alias_decl = (
        f"    CheckFactories.registerCheck<{namespace_prefix}"
        f'{current_check_class}>(\n        "{alias_fq_name}");\n'
    )

    print(f"Updating {filename}...")
    with io.open(filename, "w", encoding="utf8", newline="\n") as f:
        header_added = False
        header_found = False
        alias_added = False

        lines_iter = iter(lines)
        try:
            while True:
                line = next(lines_iter)

                # Add the include if it's a cross-module alias
                if alias_module != current_module and not header_added:
                    match = re.search(r'#include "(.*)"', line)
                    if match:
                        header_found = True
                        expected_path = f"../{current_module}/{current_check_class}.h"
                        if match.group(1) > expected_path:
                            header_added = True
                            f.write(include_line)
                    elif header_found:
                        header_added = True
                        f.write(include_line)

                if not alias_added:
                    if line.strip() == "}":
                        alias_added = True
                        f.write(alias_decl)
                    else:
                        match = re.search(
                            r'registerCheck<(.*)> *\( *(?:"([^"]*)")?', line
                        )
                        prev_line = None
                        if match:
                            current_check_name = match.group(2)
                            if current_check_name is None:
                                # Check name might be on the next line
                                prev_line = line
                                line = next(lines_iter)
                                match = re.search(r' *"([^"]*)"', line)
                                if match:
                                    current_check_name = match.group(1)
                            assert current_check_name
                            if current_check_name > alias_fq_name:
                                alias_added = True
                                f.write(alias_decl)
                            if prev_line:
                                f.write(prev_line)
                f.write(line)
        except StopIteration:
            pass


def write_alias_docs(
    alias_module_path: str,
    alias_module: str,
    alias_check: str,
    current_module: str,
    current_check: str,
) -> None:
    """Create a redirect documentation file for the alias."""
    alias_fq_name = f"{alias_module}-{alias_check}"
    current_fq_name = f"{current_module}-{current_check}"
    underline = "=" * len(alias_fq_name)

    filename = os.path.normpath(
        os.path.join(
            alias_module_path,
            "../../docs/clang-tidy/checks/",
            alias_module,
            f"{alias_check}.rst",
        )
    )
    print(f"Creating {filename}...")

    content = f""".. title:: clang-tidy - {alias_fq_name}
.. meta::
   :http-equiv=refresh: 5;URL=../{current_module}/{current_check}.html

{alias_fq_name}
{underline}

The `{alias_fq_name}` check is an alias, please see
`{current_fq_name} <../{current_module}/{current_check}.html>`_
for more information.
"""

    with io.open(filename, "w", encoding="utf8", newline="\n") as f:
        f.write(content)


def add_release_notes(
    clang_tidy_path: str,
    alias_module: str,
    alias_check: str,
    current_module: str,
    current_check: str,
) -> None:
    """Add a release notes entry."""
    alias_fq_name = f"{alias_module}-{alias_check}"
    current_fq_name = f"{current_module}-{current_check}"

    filename = os.path.normpath(
        os.path.join(clang_tidy_path, "../docs/ReleaseNotes.rst")
    )
    with io.open(filename, "r", encoding="utf8") as f:
        lines = f.readlines()

    line_matcher = re.compile("New check aliases")
    next_section_matcher = re.compile("Changes in existing checks")
    check_matcher = re.compile(r"- New :doc:`(.*)` check.")

    print(f"Updating {filename}...")

    release_note = f"""- New :doc:`{alias_fq_name}
  <clang-tidy/checks/{alias_module}/{alias_check}>` check alias
  to :doc:`{current_fq_name}
  <clang-tidy/checks/{current_module}/{current_check}>`.

"""

    with io.open(filename, "w", encoding="utf8", newline="\n") as f:
        note_added = False
        header_found = False
        add_note_here = False

        for line in lines:
            if not note_added:
                match = line_matcher.match(line)
                match_next = next_section_matcher.match(line)
                match_check = check_matcher.match(line)

                if match_check:
                    last_check = match_check.group(1)
                    if last_check > alias_fq_name:
                        add_note_here = True

                if match_next:
                    add_note_here = True

                if match:
                    header_found = True
                    f.write(line)
                    continue

                if line.startswith("^^^^"):
                    f.write(line)
                    continue

                if header_found and add_note_here and not line.startswith("^^^^"):
                    f.write(release_note)
                    note_added = True

            f.write(line)


def update_checks_list(clang_tidy_path: str) -> None:
    """Recreate the list of checks in the docs/clang-tidy/checks directory."""
    update_script = os.path.join(clang_tidy_path, "add_new_check.py")
    os.system(f"{update_script} --update-docs")


def main() -> None:
    """Main entry point for the script."""
    parser = argparse.ArgumentParser(
        description="Add a new clang-tidy check alias.",
        epilog="""
Example:
  %(prog)s cert-err58-cpp bugprone-throwing-static-initialization

This creates an alias 'cert-err58-cpp' pointing to the existing check
'bugprone-throwing-static-initialization'.
""",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("alias_name", type=str, help="Alias check name.")
    parser.add_argument("current_name", type=str, help="Current check name.")
    args = parser.parse_args()

    alias_module = args.alias_name.split("-")[0]
    alias_check = "-".join(args.alias_name.split("-")[1:])
    current_module = args.current_name.split("-")[0]
    current_check = "-".join(args.current_name.split("-")[1:])

    current_check_class = get_camel_check_name(current_check)

    clang_tidy_path = os.path.dirname(sys.argv[0])
    alias_module_path = os.path.join(clang_tidy_path, alias_module)
    current_module_path = os.path.join(clang_tidy_path, current_module)

    # Verify that the current check exists
    current_header = os.path.join(current_module_path, f"{current_check_class}.h")
    if not os.path.isfile(current_header):
        print(
            f'Current check "{args.current_name}" does not exist '
            f"(expected file: {current_header}). Exiting."
        )
        sys.exit(1)

    # Verify that the alias module exists
    if not os.path.isdir(alias_module_path):
        print(f'Alias module directory "{alias_module}" does not exist. Exiting.')
        sys.exit(1)

    register_alias_in_module(
        alias_module_path, alias_module, alias_check, current_module, current_check_class
    )
    write_alias_docs(
        alias_module_path, alias_module, alias_check, current_module, current_check
    )
    add_release_notes(
        clang_tidy_path, alias_module, alias_check, current_module, current_check
    )
    update_checks_list(clang_tidy_path)
    print(
        f"Done. Alias '{args.alias_name}' -> '{args.current_name}' "
        f"created successfully!"
    )


if __name__ == "__main__":
    main()

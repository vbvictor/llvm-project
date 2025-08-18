#!/usr/bin/env python3
#
# ====- clang-tidy-helper, runs clang-tidy from the ci --*- python -*--==#
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# ==--------------------------------------------------------------------------------------==#

import argparse
import os
import re
import shlex
import subprocess
import sys
from typing import List, Optional

"""
This script is run by GitHub actions to ensure that the code in PR's conform to
the coding style of LLVM. The canonical source of this script is in the LLVM
source tree under llvm/utils/git.

You can learn more about the LLVM coding style on llvm.org:
https://llvm.org/docs/CodingStandards.html

You can control the exact path to clang-tidy with the following environment
variables: $CLANG_TIDY_PATH.
"""


class TidyArgs:
    start_rev: str = None
    end_rev: str = None
    repo: str = None
    changed_files: List[str] = []
    token: str = None
    issue_number: int = 0
    write_comment_to_file: bool = False

    def __init__(self, args: argparse.Namespace = None) -> None:
        if not args is None:
            self.start_rev = args.start_rev
            self.end_rev = args.end_rev
            self.repo = args.repo
            #self.token = args.token
            self.changed_files = args.changed_files
            #self.issue_number = args.issue_number
            self.write_comment_to_file = args.write_comment_to_file


class TidyHelper:
    COMMENT_TAG = "<!--LLVM CLANG-TIDY COMMENT: {fmt}-->"
    name: str
    friendly_name: str
    comment: dict = None

    @property
    def comment_tag(self) -> str:
        return self.COMMENT_TAG.replace("fmt", self.name)

    @property
    def instructions(self) -> str:
        raise NotImplementedError()

    def has_tool(self) -> bool:
        raise NotImplementedError()

    def lint_run(self, changed_files: List[str], args: TidyArgs) -> Optional[str]:
        raise NotImplementedError()

    def pr_comment_text_for_diff(self, diff: str) -> str:
        return f"""
:warning: {self.friendly_name}, {self.name} found issues in your code. :warning:

<details>
<summary>
You can test this locally with the following command:
</summary>

``````````bash
{self.instructions}
``````````

</details>

<details>
<summary>
View the diff from {self.name} here.
</summary>

``````````diff
{diff}
``````````

</details>
"""

    # TODO: any type should be replaced with the correct github type, but it requires refactoring to
    # not require the github module to be installed everywhere.
    """
    def find_comment(self, pr: any) -> any:
        for comment in pr.as_issue().get_comments():
            if self.comment_tag in comment.body:
                return comment
        return None

    def update_pr(self, comment_text: str, args: TidyArgs, create_new: bool) -> None:
        import github
        from github import IssueComment, PullRequest

        repo = github.Github(args.token).get_repo(args.repo)
        pr = repo.get_issue(args.issue_number).as_pull_request()

        comment_text = self.comment_tag + "\n\n" + comment_text

        existing_comment = self.find_comment(pr)

        if args.write_comment_to_file:
            if create_new or existing_comment:
                self.comment = {"body": comment_text}
            if existing_comment:
                self.comment["id"] = existing_comment.id
            return

        if existing_comment:
            existing_comment.edit(comment_text)
        elif create_new:
            pr.as_issue().create_comment(comment_text)
    """

    def run(self, changed_files: List[str], args: TidyArgs) -> bool:
        changed_files = [arg for arg in changed_files if "third-party" not in arg]
        print(f"got changed_files: {changed_files}")
        diff = self.lint_run(changed_files, args)
        print(f"got diff {diff}")
        should_update_gh = True
        #args.token is not None and args.repo is not None

        if diff is None:
            if should_update_gh:
                comment_text = (
                    ":white_check_mark: With the latest revision "
                    f"this PR passed the {self.friendly_name}."
                )
                print(comment_text)
                # self.update_pr(comment_text, args, create_new=False)
            return True
        elif len(diff) > 0:
            if should_update_gh:
                comment_text = self.pr_comment_text_for_diff(diff)
                print(comment_text)
                # self.update_pr(comment_text, args, create_new=True)
            else:
                print(
                    f"Warning: {self.friendly_name}, {self.name} detected "
                    "some issues with your code formatting..."
                )
            return False
        else:
            # The formatter failed but didn't output a diff (e.g. some sort of
            # infrastructure failure).
            comment_text = (
                f":warning: The {self.friendly_name} failed without printing "
                "a diff. Check the logs for stderr output. :warning:"
            )
            print(comment_text)
            # self.update_pr(comment_text, args, create_new=False)
            return False


class ClangTidyDiffHelper(TidyHelper):
    name = "clang-tidy"
    friendly_name = "C/C++ code linter"

    def __init__(self, build_path: str = "build", clang_tidy_binary: str = "clang-tidy"):
        self.build_path = build_path
        self.clang_tidy_binary = clang_tidy_binary
        self.cpp_files = []

    @property
    def instructions(self) -> str:
        return f"""
git diff -U0 origin/main..HEAD -- {self.cpp_files} |
python3 clang-tools-extra/clang-tidy/tool/clang-tidy-diff.py \\
  -path {self.build_path} -p1

# See https://clang.llvm.org/extra/clang-tidy/#using-clang-tidy for more
# instructions on how to use clang-tidy"""

    def filter_changed_files(self, changed_files: List[str]) -> List[str]:
        filtered_files = []
        print(f"filtering changed files: {filtered_files}")
        for path in changed_files:
            print(f"file: {path}")
            if not path.startswith("clang-tools-extra/clang-tidy/"):
                print("continue")
                continue
            _, ext = os.path.splitext(path)
            if ext in (".cpp", ".c", ".h", ".hpp", ".hxx", ".cxx"):
                if os.path.exists(path):
                    print("appending")
                    filtered_files.append(path)
                else:
                    print("skipped")
            else:
                print(f"wrong ext {ext}")
        return filtered_files

    @property
    def clang_tidy_path(self) -> str:
        if "CLANG_TIDY_PATH" in os.environ:
            return os.environ["CLANG_TIDY_PATH"]
        return self.clang_tidy_binary

    def has_tool(self) -> bool:
        cmd = [self.clang_tidy_path, "--version"]
        proc = None
        try:
            proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except:
            return False
        return proc.returncode == 0

    def lint_run(self, changed_files: List[str], args: TidyArgs) -> Optional[str]:
        cpp_files = self.filter_changed_files(changed_files)
        if not cpp_files:
            print("no cpp files!")
            return None

        print(f"Generating diff: begin: {args.start_rev}, end: {args.end_rev}")
        git_diff_cmd = [
            "git", "diff", "-U0", 
            f"{args.start_rev}..{args.end_rev}",
            "--"
        ] + cpp_files

        print(f"Generating diff: {' '.join(git_diff_cmd)}")

        diff_proc = subprocess.run(
            git_diff_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False
        )

        if diff_proc.returncode != 0:
            print(f"Git diff failed: {diff_proc.stderr}")
            return None

        diff_content = diff_proc.stdout
        if not diff_content.strip():
            print("No diff content found")
            return None

        # Run clang-tidy-diff.py
        tidy_diff_cmd = [
            "code-lint-tools/clang-tools-extra/clang-tidy/tool/clang-tidy-diff.py",
            "-path", self.build_path,
            "-p1",
            "-quiet"
        ]

        print(f"Running clang-tidy-diff: {' '.join(tidy_diff_cmd)}")

        proc = subprocess.run(
            tidy_diff_cmd,
            input=diff_content,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False
        )

        output = proc.stdout.strip()
        if output:
            # Check if clang-tidy-diff found no relevant changes
            if output == "No relevant changes found.":
                return None  # No issues found, this is success
            # Filter out summary lines like "N warnings generated"
            lines = output.split('\n')
            filtered_lines = []
            for line in lines:
                if line.strip() and not line.endswith('generated.'):
                    filtered_lines.append(line)
            if filtered_lines:
                return '\n'.join(filtered_lines)

        return None

    def pr_comment_text_for_diff(self, warnings: str) -> str:
        return f"""
:warning: {self.friendly_name} found issues in your code. :warning:

<details>
<summary>
You can test this locally with the following command:
</summary>

``````````bash
{self.instructions}
``````````

</details>

<details>
<summary>
View the warnings from {self.name} here.
</summary>

``````````
{warnings}
``````````

</details>
"""


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    # parser.add_argument(
        # "--token", type=str, required=True, help="GitHub authentiation token"
    # )
    parser.add_argument(
        "--repo",
        type=str,
        default=os.getenv("GITHUB_REPOSITORY", "llvm/llvm-project"),
        help="The GitHub repository that we are working with in the form of <owner>/<repo> (e.g. llvm/llvm-project)",
    )
    # parser.add_argument("--issue-number", type=int, required=True)
    parser.add_argument(
        "--start-rev",
        type=str,
        required=True,
        help="Compute changes from this revision.",
    )
    parser.add_argument(
        "--end-rev", type=str, required=True, help="Compute changes to this revision"
    )
    parser.add_argument(
        "--changed-files",
        type=str,
        help="Comma separated list of files that has been changed",
    )
    parser.add_argument(
        "--write-comment-to-file",
        action="store_true",
        help="Don't post comments on the PR, instead write the comments and metadata a file called 'comment'",
    )
    parser.add_argument(
        "--build-path",
        type=str,
        default="build",
        help="Path to build directory with compile_commands.json"
    )
    parser.add_argument(
        "--clang-tidy-binary",
        type=str,
        default="clang-tidy",
        help="Path to clang-tidy binary"
    )

    parsed_args = parser.parse_args()
    args = TidyArgs(parsed_args)

    changed_files = []
    if args.changed_files:
        changed_files = args.changed_files.split(",")

    failed_linters = []
    comments = []
    clang_tidy_runner = ClangTidyDiffHelper(
        build_path=parsed_args.build_path,
        clang_tidy_binary=parsed_args.clang_tidy_binary
    )

    print("running tool")
    if not clang_tidy_runner.run(changed_files, args):
        print("adding failed")
        failed_linters.append(clang_tidy_runner.name)
    if clang_tidy_runner.comment:
        print("adding comment")
        comments.append(clang_tidy_runner.comment)

    if len(comments) > 0:
        print("dumping comments")
        with open("comments", "w") as f:
            import json

            json.dump(comments, f)
            print("dumping done!")

    if len(failed_linters) > 0:
        print(f"error: some linters failed: {' '.join(failed_linters)}")
        sys.exit(1)

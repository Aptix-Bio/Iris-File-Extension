#!/usr/bin/env python3
"""Fail when a workflow job would run anywhere but a pinned Blacksmith runner.

Ryan, 2026-11-24: "We should be using blacksmith for our CI runners". A
GitHub-hosted label (ubuntu-24.04, macos-26, windows-2025) bills GitHub
minutes, which stopped starting jobs in Aptix-Bio's private repositories that
day; a -latest label lets the image move under a passing build (Arbiter
directives/ci_runners.md, "Pin what can move").

Labels are matched as text wherever they appear outside a comment, so a label
in a matrix entry is checked the same as one written after runs-on. The
accepted labels are those Blacksmith documents
(https://docs.blacksmith.sh/blacksmith-runners/overview, read 2026-11-24).

Usage: tools/runner_labels_lint.py [--quiet]     (non-zero exit on any finding)
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORKFLOWS = ROOT / ".github" / "workflows"

BLACKSMITH = re.compile(
    r"^blacksmith-(?:"
    r"(?:2|4|8|16|32)vcpu-ubuntu-(?:2204|2404|2604)(?:-arm)?"
    r"|(?:2|4|8|16|32)vcpu-windows-2025"
    r"|(?:6|12)vcpu-macos-(?:15|26|27)"
    r")$"
)
ANY_BLACKSMITH = re.compile(r"blacksmith-[A-Za-z0-9.-]+")
# A whole word: ubuntu-24.04, macos-15, windows-2025, ubuntu-latest. A file
# name such as cmake-macos-CI.yml is not a label.
GITHUB_HOSTED = re.compile(r"(?<![A-Za-z0-9_.-])(?:ubuntu|macos|windows)-(?:[0-9]|latest)[A-Za-z0-9.-]*")
RUNS_ON = re.compile(r"runs-on:\s*([^\s$].*)$")


def _code(line: str) -> str:
    """The line without its comment: comments may name a job's former label."""
    if line.lstrip().startswith("#"):
        return ""
    return re.sub(r"\s#.*$", "", line)


def findings(workflow: Path) -> list[str]:
    out = []
    for number, line in enumerate(workflow.read_text().splitlines(), start=1):
        code = _code(line)
        where = f"{workflow.relative_to(ROOT)}:{number}"
        if "-latest" in code:
            out.append(f"{where}: a -latest label moves with the image")
        for label in ANY_BLACKSMITH.findall(code):
            if not BLACKSMITH.match(label):
                out.append(f"{where}: '{label}' is not a documented Blacksmith label")
        rest = ANY_BLACKSMITH.sub("", code)
        for label in GITHUB_HOSTED.findall(rest):
            out.append(f"{where}: '{label}' is a GitHub-hosted label")
        match = RUNS_ON.search(rest)
        if match:
            out.append(f"{where}: runs-on '{match.group(1)}' is not a Blacksmith label")
    return out


def main(argv: list[str]) -> int:
    workflows = sorted(WORKFLOWS.glob("*.yml")) + sorted(WORKFLOWS.glob("*.yaml"))
    if not workflows:
        print(f"no workflow files under {WORKFLOWS}", file=sys.stderr)
        return 1
    problems = [f for w in workflows for f in findings(w)]
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f"{len(problems)} runner label problem(s)", file=sys.stderr)
        return 1
    if "--quiet" not in argv:
        print(f"runner labels: {len(workflows)} workflow(s), every job on a pinned Blacksmith runner")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

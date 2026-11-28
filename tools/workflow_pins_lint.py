#!/usr/bin/env python3
"""Fail when a workflow could run moved code or hand a step more access than it uses.

CI-4.3 (RTM Pipeline_04), from the CI alignment audit of 2026-11-27. Three rules:
- every `uses:` of a remote action names a 40-hex commit with its version in a comment, since a
  tag can be moved to other code after a run passes;
- every workflow starts from `permissions: {}`, so each grant is made per job and a step's token
  holds only what its job was granted;
- every `actions/checkout` sets `persist-credentials: false`, so no later step finds the token
  in the checkout's git configuration.

Lines are read as text, as tools/runner_labels_lint.py reads them, so no YAML parser is needed.

Usage: tools/workflow_pins_lint.py [--quiet] [workflow ...]   (non-zero exit on any finding)
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORKFLOWS = ROOT / ".github" / "workflows"

USES = re.compile(r"^(\s*)(?:-\s+)?uses:\s*([^\s#]+)\s*(#.*)?$")
PINNED = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_./-]+@[0-9a-f]{40}$")
VERSION_COMMENT = re.compile(r"^#\s*v?\d+(?:\.\d+)*\S*\s*$")


def findings(workflow: Path) -> list[str]:
    lines = workflow.read_text().splitlines()
    name = workflow.relative_to(ROOT) if workflow.is_relative_to(ROOT) else workflow
    out = []
    if "permissions: {}" not in (line.rstrip() for line in lines):
        out.append(f"{name}: no workflow-level `permissions: {{}}`")
    for number, line in enumerate(lines, start=1):
        match = USES.match(line)
        if not match:
            continue
        indent, target, comment = match.group(1), match.group(2), match.group(3)
        where = f"{name}:{number}"
        if target.startswith("./") or target.startswith("docker://"):
            continue
        if not PINNED.match(target):
            out.append(f"{where}: '{target}' is not pinned by a 40-hex commit")
        elif not comment or not VERSION_COMMENT.match(comment):
            out.append(f"{where}: '{target}' has no version comment (# vX.Y.Z)")
        if target.startswith("actions/checkout@"):
            # The step's keys share one indent (two past its `- `); a line less indented, or the
            # next `- ` item, ends the step.
            keys = len(indent) + (2 if line.lstrip().startswith("-") else 0)
            body = []
            for following in lines[number:]:
                depth = len(following) - len(following.lstrip())
                if following.strip() and (depth < keys or following.lstrip().startswith("- ") and depth < keys + 2):
                    break
                body.append(following.strip())
            if "persist-credentials: false" not in body:
                out.append(f"{where}: actions/checkout without `persist-credentials: false`")
    return out


def main(argv: list[str]) -> int:
    named = [Path(a) for a in argv if not a.startswith("--")]
    workflows = named or sorted(WORKFLOWS.glob("*.yml")) + sorted(WORKFLOWS.glob("*.yaml"))
    if not workflows:
        print(f"no workflow files under {WORKFLOWS}", file=sys.stderr)
        return 1
    problems = [f for w in workflows for f in findings(w)]
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f"{len(problems)} workflow pin or permission problem(s)", file=sys.stderr)
        return 1
    if "--quiet" not in argv:
        print(f"workflow pins: {len(workflows)} workflow(s), every action pinned by commit, "
              "every grant per job, every checkout without credentials")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

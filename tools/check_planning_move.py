"""Check that the move of IFE's root planning documents into development/ dropped nothing.

    python3 tools/check_planning_move.py <ife checkout> [<base commit>]

Reads MIGRATION.md, TASKS.md and architecture.md at the base commit (default 005347e, the
last commit that holds them) and development/**/*.md from the working tree, then:
  1. every work-item ID in the old files appears in development/;
  2. every ID on an open box, and every open package, appears in development/WBS.md;
  3. every open box (`- [ ]`) appears in WBS.md: the text after its box, as a substring of a
     WBS line, open or now ticked;
  4. every ticked box (`- [x]`) appears in some development/ file.
Exits 1 on any miss, naming it.
"""
import re
import subprocess
import sys
from pathlib import Path

repo = Path(sys.argv[1])
base = sys.argv[2] if len(sys.argv) > 2 else "005347e"
OLD = ("MIGRATION.md", "TASKS.md", "architecture.md")
ID = re.compile(r"\b(?:RB|RC|R|XP|VL|CI)-\d+(?:\.\d+)*\b|\bOD-R\d\b")
BOX = re.compile(r"^\s*[-*+] \[(?P<state>[ xX])\] (?P<text>.*)$")
# The only rewrites the move made inside a kept line: local and sibling-checkout paths.
PATHS = [("`../FastFHIR/", "FastFHIR's `"), ("../FastFHIR/", "FastFHIR's "), ("`../Iris-Slide/", "Iris-Slide's `")]
# Packages that are open without an open box of their own (their steps are headings or prose).
OPEN_PACKAGES = ["R-3", "R-3.1", "R-3.2", "R-3.3", "R-4", "R-5", "R-6", "RC-4", "RC-5", "RC-10",
                 "VL-1", "CI-1", "CI-3", "OD-R1", "OD-R3", "OD-R4", "OD-R5"]


def norm(text: str) -> str:
    return " ".join(text.split())


def old(name: str) -> list[str]:
    return subprocess.run(["git", "-C", str(repo), "show", f"{base}:{name}"], capture_output=True, text=True,
                          check=True).stdout.splitlines()


new_files = sorted((repo / "development").rglob("*.md"))
new_lines = {path: [norm(line) for line in path.read_text(encoding="utf-8").splitlines()] for path in new_files}
wbs = new_lines[repo / "development" / "WBS.md"]
everything = "\n".join(line for lines in new_lines.values() for line in lines)
wbs_text = "\n".join(wbs)

def box_problem(where: str, state: str, text: str) -> str | None:
    """Where a box of the old files should be now, or None when it is there: an open box in
    WBS.md, a ticked box in any development/ file."""
    for before, after in PATHS:
        text = text.replace(before, after)
    text = norm(text)
    if state == " ":
        if any(text in candidate for candidate in wbs):
            return None
        return f"open box {where} is not in WBS.md: {text[:90]!r}"
    if any(text in candidate for lines in new_lines.values() for candidate in lines):
        return None
    return f"ticked box {where} is in no development/ file: {text[:90]!r}"


problems: list[str] = []
ids: set[str] = set()
open_ids: set[str] = set(OPEN_PACKAGES)
boxes = {" ": 0, "x": 0}
for name in OLD:
    for number, line in enumerate(old(name), 1):
        found = set(ID.findall(line))
        ids |= found
        box = BOX.match(line)
        if box is None:
            continue
        state = box["state"].lower()
        boxes[state] += 1
        if state == " ":
            open_ids |= found
        problem = box_problem(f"{name}:{number}", state, box["text"])
        if problem is not None:
            problems.append(problem)

for item in sorted(ids):
    if not re.search(rf"(?<![\w.-]){re.escape(item)}(?![\w-]|\.\d)", everything):
        problems.append(f"ID {item} is in no development/ file")
for item in sorted(open_ids):
    if not re.search(rf"(?<![\w.-]){re.escape(item)}(?![\w-]|\.\d)", wbs_text):
        problems.append(f"open ID {item} is not in WBS.md")

print(f"base {base}: {len(ids)} work-item IDs, {len(open_ids)} of them open; "
      f"{boxes[' ']} open boxes, {boxes['x']} ticked boxes in {', '.join(OLD)}")
print(f"new: {len(new_files)} files under development/")
for problem in problems:
    print("MISSING:", problem)
print("OK: nothing dropped" if not problems else f"{len(problems)} problem(s)")
sys.exit(1 if problems else 0)

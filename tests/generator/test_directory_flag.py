"""`generate_IFE.py` runs from any directory; `-d DIR` names the root.

generate_IFE.py is the generator's one entry point. --schema-dir, --out-dir
and --docs-dir resolve against -d, which defaults to the repository that holds
the script, so a caller elsewhere (spec/build_document.sh run by another
repository's build, an IDE, a script) runs it by path without changing
directory or setting PYTHONPATH. With -d the caller names another root.
Absolute paths are left as they are.

Run from a temporary directory, with no PYTHONPATH, which is how a caller
outside the root reaches it. Stdlib-only, like test_determinism.py: a plain
script that exits non-zero on failure.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]
_ENTRY_POINT = _REPO_ROOT / "generate_IFE.py"


def _run(cwd: Path, *args: str) -> subprocess.CompletedProcess[str]:
    env = {key: value for key, value in os.environ.items() if key != "PYTHONPATH"}
    return subprocess.run([sys.executable, str(_ENTRY_POINT), *args],
                          cwd=cwd, env=env, capture_output=True, text=True)


def main() -> int:
    failures: list[str] = []
    with tempfile.TemporaryDirectory() as scratch:
        elsewhere = Path(scratch)

        # With no -d, the default --schema-dir, spec/, resolves against the
        # repository holding the script rather than against the working
        # directory, which has no spec/.
        result = _run(elsewhere, "--validate")
        if result.returncode != 0:
            failures.append(f"--validate from elsewhere exited {result.returncode}:\n{result.stderr}")

        # Relative output directories land under -d; an absolute schema
        # directory is used as given.
        target = elsewhere / "target"
        target.mkdir()
        result = _run(elsewhere, "-d", str(target), "--schema-dir", str(_REPO_ROOT / "spec"),
                      "--out-dir", "src", "--docs-dir", "docs")
        if result.returncode != 0:
            failures.append(f"generation with -d exited {result.returncode}:\n{result.stderr}")
        for expected in (target / "src" / "IFE_Blocks.hpp", target / "docs" / "attributes.adoc"):
            if not expected.is_file():
                failures.append(f"expected {expected.relative_to(elsewhere)} under -d, not written")
        if (elsewhere / "src").exists() or (elsewhere / "docs").exists():
            failures.append("relative output directories resolved against the working directory instead of -d")

    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    if not failures:
        print("generate_IFE.py: runs from any directory; relative paths resolve against -d")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

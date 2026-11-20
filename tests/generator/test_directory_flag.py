"""`python3 -m generator -d DIR` resolves its relative paths against DIR.

Without -d, --schema-dir, --out-dir and --docs-dir are relative to the
current directory, so the generator works only when run from the repository
root, and every caller elsewhere (spec/build_document.sh run by another
repository's build, an IDE, a script) has to change directory first. With -d
the caller names the root instead. Absolute paths are left as they are.

Run from a temporary directory with PYTHONPATH naming the repository, which
is how a caller outside the root reaches the module. Stdlib-only, like
test_determinism.py: a plain script that exits non-zero on failure.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]


def _run(cwd: Path, *args: str) -> subprocess.CompletedProcess[str]:
    env = dict(os.environ, PYTHONPATH=str(_REPO_ROOT))
    return subprocess.run([sys.executable, "-m", "generator", *args],
                          cwd=cwd, env=env, capture_output=True, text=True)


def main() -> int:
    failures: list[str] = []
    with tempfile.TemporaryDirectory() as scratch:
        elsewhere = Path(scratch)

        # The default --schema-dir, spec/, resolved against -d rather than
        # against a directory that has no spec/.
        result = _run(elsewhere, "-d", str(_REPO_ROOT), "--validate")
        if result.returncode != 0:
            failures.append(f"-d ROOT --validate from elsewhere exited {result.returncode}:\n{result.stderr}")

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
        print("generator -d: relative paths resolve against the named directory")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

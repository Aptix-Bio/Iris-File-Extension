#!/usr/bin/env python3
"""Generate the Iris File Extension's C++ layer and specification tables.

The one entry point to the generator (generator/, standard library only).
It puts the repository root on sys.path, so it runs by path from any working
directory with no PYTHONPATH, and hands its arguments to generator.cli:

    python3 generate_IFE.py --validate     # the spec documents agree; nothing written
    python3 generate_IFE.py                # build/generated_source/ and build/generated_docs/
    python3 generate_IFE.py --check        # those outputs and the wire witness are current
    python3 generate_IFE.py --out-dir DIR --docs-dir DIR

Relative paths resolve against -d, which defaults to this repository. CMake
runs it at every configure into its own binary directory; the flags are in
generator/README.md.
"""
import sys
from pathlib import Path

# absolute(), not resolve(): see generator.cli.REPOSITORY_ROOT.
sys.path.insert(0, str(Path(__file__).absolute().parent))

from generator.cli import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())

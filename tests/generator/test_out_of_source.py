"""A configured build writes the generated layer into its own tree.

The generated C++ and documents are build products of spec/*.json. Written
into the source tree, they sit beside committed files where git, an editor or
a careless `git add` treats them as source, and every build directory of one
checkout shares, and overwrites, one copy. CMake therefore generates into
${CMAKE_CURRENT_BINARY_DIR}, as Bazel's genrule writes into $(RULEDIR), and
the release publishes the same output as an archive (R-7).

ctest passes the directory CMake compiled the layer from (IFE_GENERATED_DIR)
and the source root. This checks that the layer is there, that the directory
is not the source tree's, and that the source tree has no generated_source/
or generated_docs/ at all. Stdlib-only, like test_determinism.py: a plain
script that exits non-zero on failure.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Generation's anchor: the header every consumer includes.
_ANCHOR = "IFE_Blocks.hpp"
_SOURCE_TREE_OUTPUTS = ("generated_source", "generated_docs")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--source", type=Path, required=True,
                        help="the repository root")
    parser.add_argument("--generated", type=Path, required=True,
                        help="the directory CMake compiled the generated layer from")
    args = parser.parse_args()

    source = args.source.resolve()
    generated = args.generated.resolve()
    failures: list[str] = []
    if not (generated / _ANCHOR).is_file():
        failures.append(f"{generated / _ANCHOR} was not generated")
    for name in _SOURCE_TREE_OUTPUTS:
        if generated == source / name:
            failures.append(f"the build compiles the generated layer from the source tree: {generated}")
        if (source / name).exists():
            failures.append(f"the source tree holds {source / name}")

    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    if not failures:
        print(f"generated layer in {generated}; the source tree holds none")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

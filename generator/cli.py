"""IFE code generator — command-line interface, run through generate_IFE.py.

Emits the C++ serialization layer (block handles, enumerations, sentinels,
the conformance layer) and the specification's generated tables from the
committed spec JSON in `spec/`.

The flags are a contract: CMake's configure step, Bazel's genrules,
spec/build_document.sh and CI invoke exactly this command line. Do not change
them without updating those callers and generator/README.md in the same
change.

-d/--directory names the directory the three relative paths resolve against.
It defaults to the repository holding this package, so a caller in any
working directory (another repository's build, an IDE) runs
generate_IFE.py by path and the default spec/ is found. The default output
directories, build/generated_source and build/generated_docs, are where
`cmake -B build` writes them; CMake and Bazel pass their own absolute or
-d-relative paths.
"""
from __future__ import annotations

import argparse
from pathlib import Path

from . import pipeline

# The repository root: the package's parent directory. absolute(), not
# resolve(): under Bazel's sandbox the package is reached through symlinks,
# and resolving them would point -d at the real checkout outside the sandbox.
REPOSITORY_ROOT = Path(__file__).absolute().parents[1]
# Below -d; gitignored with the rest of build/.
DEFAULT_OUT_DIR = "build/generated_source"
DEFAULT_DOCS_DIR = "build/generated_docs"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="generate_IFE.py",
        description="Generate the IFE C++ serialization layer and derived layout tables.",
    )
    parser.add_argument(
        "-d", "--directory",
        default=str(REPOSITORY_ROOT),
        help="Directory the relative --schema-dir, --out-dir and --docs-dir resolve against "
             "(default: the repository root). Absolute paths are used as given",
    )
    parser.add_argument(
        "--schema-dir",
        default="spec",
        help="Directory holding ife_fields.json and ife_constants.json (default: spec/)",
    )
    parser.add_argument(
        "--out-dir",
        default=DEFAULT_OUT_DIR,
        help=f"Directory to write generated C++ into (default: {DEFAULT_OUT_DIR}/)",
    )
    parser.add_argument(
        "--docs-dir",
        default=DEFAULT_DOCS_DIR,
        help=f"Directory to write generated documentation into (default: {DEFAULT_DOCS_DIR}/)",
    )
    parser.add_argument(
        "--validate",
        action="store_true",
        help="Check the spec documents for conflicts and dangling references (exit 1 on any)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Verify outputs match a fresh regeneration (exit 1 on drift)",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    # Joining onto the directory leaves an absolute path as it is.
    root = Path(args.directory)
    return pipeline.run(
        schema_dir=root / args.schema_dir,
        out_dir=root / args.out_dir,
        docs_dir=root / args.docs_dir,
        check=args.check,
        validate_only=args.validate,
    )

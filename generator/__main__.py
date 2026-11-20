"""IFE code generator — command-line entry point.

Emits the C++ serialization layer
(vtables, enums, sentinels) and the derived doc tables from the committed
spec JSON in `spec/`, into `generated_source/` and `generated_docs/`.

The CLI contract here is final: CMake's configure-time generation
(IFE_RUN_GENERATOR, or auto when generated_source/ lacks its anchor file)
invokes exactly this command line. Do not change flags without updating
CMakeLists.txt and generator/README.md in the same change.

-d/--directory names the directory the three relative paths resolve against,
so a caller outside the repository root (another repository's build, an IDE)
names the root instead of changing into it. Optional, defaulting to the
current directory, so CMake's invocation is unchanged.
"""
import argparse
import sys
from pathlib import Path

from . import pipeline


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="python -m generator",
        description="Generate the IFE C++ serialization layer and derived layout tables.",
    )
    parser.add_argument(
        "-d", "--directory",
        default=".",
        help="Directory the relative --schema-dir, --out-dir and --docs-dir resolve against "
             "(default: the current directory). Absolute paths are used as given",
    )
    parser.add_argument(
        "--schema-dir",
        default="spec",
        help="Directory holding ife_fields.json and ife_constants.json (default: spec/)",
    )
    parser.add_argument(
        "--out-dir",
        default="generated_source",
        help="Directory to write generated C++ into (default: generated_source/)",
    )
    parser.add_argument(
        "--docs-dir",
        default="generated_docs",
        help="Directory to write generated documentation into (default: generated_docs/)",
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


def main(argv=None) -> int:
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


if __name__ == "__main__":
    sys.exit(main())

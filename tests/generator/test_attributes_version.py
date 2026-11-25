"""ATTRIBUTES.VERSION under METADATA_I2S is the I2S version, packed.

Decided 2026-11-25: the u16 VERSION of an ATTRIBUTES block that declares
METADATA_I2S holds the Iris Interoperability Specification version it conforms
to as `major << 8 | minor`, so I2S 0.1 is 0x0001 and I2S 1.0 is 0x0100. Zero is
unversioned, which a reader already takes as free text (ARCHITECTURE D17). A
writer and a reader that packed the two halves differently would each read the
other's claim as a different version, so the rule is stated once, normatively,
in the schema and the narrative. This pins four things:

  * the committed schema states the packing as a "shall" on VERSION, with how
    a reader unpacks it, and METADATA_I2S names the same packing;
  * the emitted document tables carry both, since the published document
    renders the schema's descriptions rather than retyping them;
  * the narrative's ife-attributes clause states it normatively;
  * the rule is prose about a value, not a wire fact: the witness is identical
    with and without it, so it never forces a refresh of
    tests/wire/witness.json.

Stdlib-only, like test_deprecated.py: a plain script that exits non-zero on
failure.
"""

from __future__ import annotations

import copy
import json
import sys
from pathlib import Path
from typing import Any

_REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(_REPO_ROOT))

from generator.emit.docs import emit_block_layout, emit_constants_table  # noqa: E402
from generator.model.layout import RECOVERY_PREFIX, derive_layout  # noqa: E402
from generator.witness import witness  # noqa: E402

# The rule as the schema and the narrative spell it, and the two worked
# examples the decision names.
_PACKING = "major << 8 | minor"
_UNPACK_MAJOR = "VERSION >> 8"
_UNPACK_MINOR = "VERSION & 0xFF"
_EXAMPLES = ("0x0001", "0x0100")


def _load(name: str) -> dict[str, Any]:
    with open(_REPO_ROOT / "spec" / name, encoding="utf-8") as handle:
        loaded: dict[str, Any] = json.load(handle)
    return loaded


def _version_field(fields: dict[str, Any]) -> dict[str, Any]:
    for group in fields["blocks"]["ATTRIBUTES"]["fields"]["ife_version"].values():
        for field in group:
            if field["name"] == "VERSION":
                found: dict[str, Any] = field
                return found
    raise KeyError("ATTRIBUTES.VERSION")


def _i2s_member(constants: dict[str, Any]) -> dict[str, Any]:
    for members in constants["metadata_formats"]["ife_version"].values():
        if "METADATA_I2S" in members:
            found: dict[str, Any] = members["METADATA_I2S"]
            return found
    raise KeyError("metadata_formats.METADATA_I2S")


def _row(table: str, name: str) -> str:
    return next((line for line in table.splitlines() if line.startswith(f"|`{name}`")), "")


def _attributes_clause() -> str:
    """The narrative from the ife-attributes anchor to the next section."""
    text = (_REPO_ROOT / "spec" / "ife_spec.adoc").read_text(encoding="utf-8")
    start = text.index("[[ife-attributes]]")
    end = text.index("\n=== ", start)
    return text[start:end]


def main() -> int:
    failures: list[str] = []
    fields = _load("ife_fields.json")
    constants = _load("ife_constants.json")
    header = _load("ife_header.json")

    # ---- the committed schema states the rule ---------------------------- #
    version = str(_version_field(fields).get("description", ""))
    for expected in ("shall", _PACKING, _UNPACK_MAJOR, _UNPACK_MINOR, *_EXAMPLES):
        if expected not in version:
            failures.append(f"ATTRIBUTES.VERSION's description lacks {expected!r}: {version!r}")
    i2s = str(_i2s_member(constants).get("description", ""))
    if _PACKING not in i2s:
        failures.append(f"METADATA_I2S's description lacks {_PACKING!r}: {i2s!r}")

    # ---- the emitted tables carry it ------------------------------------- #
    # A table cell escapes the AsciiDoc column separator, so `|` reads `\|`.
    cell = _PACKING.replace("|", "\\|")
    layout = derive_layout(fields, constants)
    attributes = emit_block_layout(layout.blocks["ATTRIBUTES"], "0" * 64)
    if cell not in _row(attributes, "VERSION"):
        failures.append(f"the ATTRIBUTES table's VERSION row lacks {cell!r}")
    formats = emit_constants_table(
        "metadata_formats", constants["metadata_formats"], RECOVERY_PREFIX, "0" * 64
    )
    if cell not in _row(formats, "METADATA_I2S"):
        failures.append(f"the metadata formats table's METADATA_I2S row lacks {cell!r}")

    # ---- the narrative states it normatively ----------------------------- #
    clause = _attributes_clause()
    for expected in ("*shall*", _PACKING, _UNPACK_MAJOR, _UNPACK_MINOR, *_EXAMPLES):
        if expected not in clause:
            failures.append(f"spec/ife_spec.adoc's ife-attributes clause lacks {expected!r}")

    # ---- the rule is not a wire change ------------------------------------ #
    unstated = copy.deepcopy(fields)
    _version_field(unstated)["description"] = "Version of the standard named by FORMAT."
    if witness(fields, constants, header) != witness(unstated, constants, header):
        failures.append("the witness changes with VERSION's description; prose must not move a wire fact")

    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    if not failures:
        print("attributes version: I2S packed major << 8 | minor, stated, emitted, and off the wire")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

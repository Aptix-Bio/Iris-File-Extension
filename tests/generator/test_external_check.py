"""`external_check`: IFE's blocks cross-checked against an external stream.

IFE does not validate an external specification's stream (decided
2026-11-26, D31). The optional `external_check` section of
spec/ife_fields.json lists, per external specification, the syntax elements a
reader takes from a tile's stream and the IFE field each must agree with; the
generator emits the comparisons and their diagnostics from it, so the JSON
stays the one source of the rule (D1). This pins four things:

  * the committed schema's AV1 section: the tile encoding it applies under,
    and its checks, in order, each naming the IFE field, the AV1 syntax
    element, its section, the relation and the clause;
  * `validate()` refuses a malformed section -- an unknown relation, an IFE
    field no block declares, a tile encoding the schema does not declare, a
    clause the narrative does not anchor, a missing section or operand, a key
    outside the closed vocabulary (`requirement` among them: the rule's prose
    lives in the specification and the field descriptions, and every
    diagnostic is generated from the entry's fields);
  * the emitters carry it: IFE_Blocks declares the fields struct and the
    check and defines each comparison, the validation layer's
    check_tile_stream reads the stream under that tile encoding alone, and a
    schema without the section emits neither;
  * a cross-check is not a wire change: the witness is identical with and
    without the section.

Stdlib-only, like test_determinism.py: a plain script that exits non-zero on
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

from generator.emit.cpp import (  # noqa: E402
    emit_blocks_header,
    emit_blocks_source,
    emit_validation_source,
)
from generator.model.layout import derive_layout  # noqa: E402
from generator.validate import validate  # noqa: E402
from generator.witness import witness  # noqa: E402

# The AV1 checks, in the order the schema lists them: (IFE field, AV1 syntax
# element, AV1 section, relation).
_AV1_CHECKS = [
    ("TILE_TABLE.TILE_LENGTH", "max_frame_width_minus_1", "5.5.1", "plus_one_equals"),
    ("TILE_TABLE.TILE_LENGTH", "max_frame_height_minus_1", "5.5.1", "plus_one_equals"),
    ("TILE_TABLE.TILE_LENGTH", "enable_superres", "5.5.1", "equals"),
    ("TILE_TABLE.ENCODING", "seq_profile", "5.5.1", "one_of"),
    ("TILE_TABLE.ENCODING", "high_bitdepth", "5.5.2", "equals"),
    ("TILE_TABLE.ENCODING", "mono_chrome", "5.5.2", "equals"),
    ("TILE_TABLE.ENCODING", "color_description_present_flag", "5.5.2", "equals"),
    ("TILE_TABLE.ENCODING", "matrix_coefficients", "5.5.2", "equals"),
    ("TILE_TABLE.ENCODING", "color_range", "5.5.2", "equals"),
    ("TILE_PIXEL_DATA.Z_PLANES", "still_picture", "5.5.1", "equals_when_ife_above"),
]

# Each comparison the emitted check_av1_stream must make.
_AV1_COMPARISONS = [
    "fields.max_frame_width_minus_1 + 1 != tile_length",
    "fields.max_frame_height_minus_1 + 1 != tile_length",
    "fields.enable_superres != 0",
    "fields.seq_profile != 0 && fields.seq_profile != 1",
    "fields.high_bitdepth != 0",
    "fields.mono_chrome != 0",
    "fields.color_description_present_flag != 1",
    "fields.matrix_coefficients != 6",
    "fields.color_range != 1",
    "z_planes > 1 && fields.still_picture != 0",
]


def _load(name: str) -> dict[str, Any]:
    with open(_REPO_ROOT / "spec" / name, encoding="utf-8") as handle:
        loaded: dict[str, Any] = json.load(handle)
    return loaded


def main() -> int:
    failures: list[str] = []
    fields = _load("ife_fields.json")
    constants = _load("ife_constants.json")
    header = _load("ife_header.json")
    narrative = (_REPO_ROOT / "spec" / "ife_spec.adoc").read_text(encoding="utf-8")

    # ---- the committed schema's AV1 section ------------------------------ #
    av1 = fields.get("external_check", {}).get("AV1", {})
    if av1.get("under") != "TILE_ENCODING_AV1":
        failures.append(f"external_check.AV1 does not apply under TILE_ENCODING_AV1: {av1.get('under')!r}")
    listed = [
        (c.get("ife_field"), c.get("field"), c.get("section"), c.get("relation"))
        for c in av1.get("checks", [])
    ]
    if listed != _AV1_CHECKS:
        failures.append(f"external_check.AV1 lists {listed}, expected {_AV1_CHECKS}")
    if any("requirement" in c for c in av1.get("checks", [])):
        failures.append("an AV1 check carries requirement prose; diagnostics come from its fields")
    if any(c.get("clause") != "ife-tile-av1" for c in av1.get("checks", [])):
        failures.append("an AV1 check does not cite clause ife-tile-av1")
    if not av1:
        # Nothing below can run without the section.
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    problems, _ = validate(fields, constants, header, narrative)
    if problems:
        failures.append(f"the committed schema does not validate: {problems}")

    # ---- validate() refuses a malformed section -------------------------- #
    def broken(edit: Any) -> dict[str, Any]:
        copied = copy.deepcopy(fields)
        edit(copied["external_check"]["AV1"])
        return copied

    malformed = {
        "an unknown relation": lambda s: s["checks"][0].update(relation="approximately"),
        "an IFE field no block declares": lambda s: s["checks"][0].update(ife_field="TILE_TABLE.TILE_EDGE"),
        "an undeclared tile encoding": lambda s: s.update(under="TILE_ENCODING_AVIF"),
        "an unanchored clause": lambda s: s["checks"][0].update(clause="ife-tile-av2"),
        "no section": lambda s: s["checks"][0].pop("section"),
        "a requirement key": lambda s: s["checks"][0].update(requirement="be TILE_LENGTH - 1"),
        "equals with no value": lambda s: s["checks"][2].pop("value"),
        "one_of with no values": lambda s: s["checks"][3].pop("values"),
        "equals_when_ife_above with no threshold": lambda s: s["checks"][9].pop("threshold"),
        "an unknown key": lambda s: s["checks"][0].update(tolerance=1),
    }
    for label, edit in malformed.items():
        problems, _ = validate(broken(edit), constants, header, narrative)
        if not any("external_check" in p for p in problems):
            failures.append(f"validate() accepted an external check with {label}")

    # ---- the emitters carry it ------------------------------------------- #
    layout = derive_layout(fields, constants)
    types = fields.get("types", {})
    section = fields["external_check"]
    hpp = emit_blocks_header(layout, types, constants, header, "0" * 64, external_check=section)
    cpp = emit_blocks_source(layout, types, header, "0" * 64, external_check=section)
    layer = emit_validation_source(layout, constants, types, header, "0" * 64, external_check=section)
    if "struct Av1StreamFields {" not in hpp:
        failures.append("IFE_Blocks.hpp does not declare Av1StreamFields")
    for _, element, _, _ in _AV1_CHECKS:
        if f"std::uint64_t {element} = 0;" not in hpp:
            failures.append(f"Av1StreamFields has no member {element}")
    if "Status check_av1_stream(const Av1StreamFields& fields" not in hpp:
        failures.append("IFE_Blocks.hpp does not declare check_av1_stream")
    for comparison in _AV1_COMPARISONS:
        if f"if ({comparison})" not in cpp:
            failures.append(f"check_av1_stream does not test `{comparison}`")
    if "::Iris::File::detail::read_av1_stream(stream, size, fields, hooks)" not in layer:
        failures.append("check_tile_stream does not read the AV1 stream")
    if layer.count("read_av1_stream(") != 1:
        failures.append("check_tile_stream reads the AV1 stream under more than one tile encoding")

    bare = copy.deepcopy(fields)
    bare.pop("external_check")
    bare_hpp = emit_blocks_header(layout, types, constants, header, "0" * 64, external_check={})
    if "Av1StreamFields" in bare_hpp:
        failures.append("a schema without external_check still emits Av1StreamFields")
    problems, _ = validate(bare, constants, header, narrative)
    if problems:
        failures.append(f"a schema without the optional section does not validate: {problems}")

    # ---- a cross-check is not a wire change ------------------------------ #
    if witness(fields, constants, header) != witness(bare, constants, header):
        failures.append("the witness changes with external_check; a cross-check must not move a wire fact")

    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    if not failures:
        print("external_check: the AV1 section validates, is emitted, and is off the wire")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

"""An enum member retired with `deprecated` keeps its value and says so.

The specification is append-only, so a value is retired rather than removed:
the member stays assigned, a file that already carries it still reads, and the
value is never reused. `deprecated` records the version that retired it and
why. This pins four things:

  * the committed schema retires IMAGE_ENCODING_PNG and ANNOTATION_PNG at 1.1
    (Ryan, 2026-11-24: "SVG preferred and let's just cut the PNG then. Simple
    is better.");
  * `validate()` refuses a malformed retirement -- no reason, a version that
    does not parse, one newer than the document or older than the member
    itself, or a key outside the closed vocabulary;
  * the emitters carry it: the C++ layer names each member and its retirement
    (`member_name`, `deprecated_since`), so a writer refuses a retired value
    from the schema instead of from a copied literal, and the published table
    states it beside the value;
  * a retirement is not a wire change: the witness is identical with and
    without it, so it never forces a refresh of tests/wire/witness.json.

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

from generator.emit.cpp import emit_blocks_header  # noqa: E402
from generator.emit.docs import emit_constants_table  # noqa: E402
from generator.model.layout import RECOVERY_PREFIX, derive_layout  # noqa: E402
from generator.validate import validate  # noqa: E402
from generator.witness import witness  # noqa: E402

_RETIRED = {
    "image_encodings": "IMAGE_ENCODING_PNG",
    "annotation_types": "ANNOTATION_PNG",
}


def _load(name: str) -> dict[str, Any]:
    with open(_REPO_ROOT / "spec" / name, encoding="utf-8") as handle:
        loaded: dict[str, Any] = json.load(handle)
    return loaded


def _member(constants: dict[str, Any], group: str, name: str) -> dict[str, Any]:
    for members in constants[group]["ife_version"].values():
        if name in members:
            found: dict[str, Any] = members[name]
            return found
    raise KeyError(f"{group}.{name}")


def main() -> int:
    failures: list[str] = []
    fields = _load("ife_fields.json")
    constants = _load("ife_constants.json")
    header = _load("ife_header.json")

    # ---- the committed schema retires PNG at 1.1 ------------------------- #
    for group, name in _RETIRED.items():
        retired = _member(constants, group, name).get("deprecated")
        if not isinstance(retired, dict) or retired.get("version") != "1.1":
            failures.append(f"{group}.{name} is not deprecated at 1.1 (found {retired!r})")

    problems, _ = validate(fields, constants, header)
    if problems:
        failures.append(f"the committed schema does not validate: {problems}")

    # ---- validate() refuses a malformed retirement ----------------------- #
    malformed: dict[str, Any] = {
        "no reason": {"version": "1.1"},
        "empty reason": {"version": "1.1", "reason": ""},
        "unparsable version": {"version": "one", "reason": "r"},
        "newer than the document": {"version": "9.0", "reason": "r"},
        "older than the member": {"version": "1.0", "reason": "r"},
        "unknown key": {"version": "1.1", "reason": "r", "replacement": "SVG"},
        "not an object": "1.1",
    }
    for label, retirement in malformed.items():
        broken = copy.deepcopy(constants)
        # CLINICAL_HL7_V2 is introduced in 1.1, so a 1.0 retirement predates it.
        _member(broken, "clinical_encodings", "CLINICAL_HL7_V2")["deprecated"] = retirement
        problems, _ = validate(fields, broken, header)
        if not any("CLINICAL_HL7_V2" in p and "deprecated" in p for p in problems):
            failures.append(f"validate() accepted a deprecation with {label}: {retirement!r}")

    # ---- the emitters carry it ------------------------------------------- #
    layout = derive_layout(fields, constants)
    cpp = emit_blocks_header(layout, fields.get("types", {}), constants, header, "0" * 64)
    for group, name in _RETIRED.items():
        enum = "".join(part.capitalize() for part in group.split("_"))
        for expected in (
            f"case {enum}::{name}: return \"{name}\";",
            f"case {enum}::{name}: return \"1.1\";",
        ):
            if expected not in cpp:
                failures.append(f"the C++ layer lacks `{expected}`")
        table = emit_constants_table(group, constants[group], RECOVERY_PREFIX, "0" * 64)
        row = next((line for line in table.splitlines() if line.startswith(f"|`{name}`")), "")
        if "Deprecated since 1.1" not in row:
            failures.append(f"the {group} table does not state that {name} is deprecated: {row!r}")
    if "member_name(MetadataFormats __v)" not in cpp:
        failures.append("member_name is not emitted for every enumeration (MetadataFormats)")

    # ---- a retirement is not a wire change ------------------------------- #
    unretired = copy.deepcopy(constants)
    for group, name in _RETIRED.items():
        _member(unretired, group, name).pop("deprecated", None)
    if witness(fields, constants, header) != witness(fields, unretired, header):
        failures.append("the witness changes with a deprecation; retirement must not move a wire fact")

    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    if not failures:
        print("deprecated: PNG retired at 1.1, validated, emitted, and off the wire")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

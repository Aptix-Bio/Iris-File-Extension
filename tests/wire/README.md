# tests/wire — the append-only witness

`witness.json` is the wire witness: a snapshot of every fact the IFE
specification documents that actually reaches the wire — block recovery tag
values, primitive inheritance, every field's (name, type, width, offset) per
version group, array entry sizes per version group, enum members and
statically defined values — derived from `spec/*.json` through
`generator.model.layout.derive_layout`, the same derivation the emitters and
the reader use. `python3 -m generator --validate` recomputes it and fails if
anything a shipped revision recorded has changed or disappeared; additions
pass silently, because that is what append-only means.

This is **append-only evidence, not a snapshot to refresh casually.** A
shipped field that moves, widens, or retypes is invisible to every
within-document check in `generator/validate.py`; this file exists to make
that diff visible at `--validate` time. Refresh it deliberately — and only
after a reviewed wire change — with:

```bash
python3 tools/refresh_witness.py
```

The tool refuses to record a wire break (a shipped fact changed or
disappeared) and prints the diff of what it is recording, so growing the
evidence is a visible act rather than a silent overwrite. When the spec
grows and the baseline has not been refreshed, `--validate` prints a
non-fatal note about the unrecorded facts and `--check` fails until the
refresh lands — the refresh is part of the change that grows the spec.

One caveat: this baseline records today's tree, which already contains the
deliberate 1.0 `ATTRIBUTE_SIZE` correction — KIND added, entry six to seven
bytes, commit `9e591fa5e9e71274cde3de5415fa55df6c82f033` — so the gate starts
from a state whose history includes the first break of the append-only
invariant, and must not be read as proof it has held unbroken.

A second deliberate break, decided 2026-11-25 (D26): `tile_encodings` value 3,
a member of the ratified 1.0 group, was renamed from `TILE_ENCODING_AVIF` to
`TILE_ENCODING_AV1` and given a new meaning (a bare AV1 Low Overhead Bitstream
Format sequence) in place of being deprecated beside a new value. It is a
reviewed wire correction that the code owner sanctioned on 2026-11-25 as an
exception to append-only (D2) and to the review rubric's SpecLedger item,
because no file carrying the AVIF tile encoding exists to break: IFE has no
deployed corpus of slides, and the hosted conformance fixtures carry
`TILE_ENCODING_JPEG` and `TILE_ENCODING_IRIS` tiles only. A future rename of a
shipped fact needs the same: a code-owner decision recorded here. The refresh
tool has no path that records a break, by design, so the one witness key was
renamed by hand in the same change; the value, 3, is unchanged, and
`--validate` passes against it.

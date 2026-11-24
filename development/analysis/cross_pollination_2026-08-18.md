# Cross-pollination work orders XP-1 to XP-4

> **Record.** Written 2026-08-18 in MIGRATION.md at the repository root; moved here on
> 2026-11-24 when `development/` replaced the root planning documents (git history
> keeps the originals; the last commit that holds them is 005347e). Kept as written,
> with local paths, sibling-checkout paths and a quotation restated for a public
> repository. Open items are tracked in [WBS.md](../WBS.md) under the same IDs; their
> boxes here read `(open: WBS.md)`. Dated records keep the names they had (Iris-Codec
> is now Iris-Slide).


Written 2026-08-18 after a joint review of this repository and FastFHIR
(FastFHIR), which share an architecture — offset-addressed blocks in a
mapped arena, recovery tags, a Python generator emitting a gitignored C++
layer, append-only wire ledgers — and have solved overlapping problems in
different orders. Each repo has something the other is missing. **These are
the items FastFHIR already has and this repository does not.** The reciprocal
list lives at the top of FastFHIR's `TASKS.md`.

**All four are done (2026-08-18).** Kept for the rules block below, which
the release orders above inherit, and for the record of what each bought.

## Rules (override anything else in this file)

1. **One task ID per session** (e.g. `XP-1.2`). Do not batch.
2. **Run the *Locate* block first.** If its output does not match *Expect*,
   **STOP** and report what you saw. A mismatch means the tree moved and the
   task is stale — do not improvise.
3. **Do not commit.** Leave changes in the working tree and report.
4. **Never hand-edit `generated_source/` or `generated_docs/`.** Change the
   emitter in `generator/emit/` and regenerate.
5. **Never move, resize or retype a shipped field.** Append-only is the
   invariant these tasks exist to *enforce*; a task that seems to require
   breaking it is a task written wrong — stop and say so.
6. A task is done only when every *Done when* criterion holds and the stated
   command exits 0. Paste the output.
7. ⚠ marks a decision a flash model must not make alone. Produce the analysis
   and STOP.

## Priority summary

| ID | Priority | Task | Why | Status |
|---|---|---|---|---|
| XP-1 | **P0** | Port FastFHIR's wire witness → enforce append-only | The invariant every compatibility claim rests on is still unenforced | **done (2026-08-18)** |
| XP-2 | **P1** | Rebuild `--check` on parity, not byte-equality | Phase 3 lists this as future work; XP-1 supplies the mechanism | **done (2026-08-18)** |
| XP-3 | P2 | Port the determinism test | Generator reordering silently changes output today | **done (2026-08-18)** |
| XP-4 | P2 | Port the dangling-view test | `ByteSpan` has the lifetime hazard the FastFHIR test was written for | **done (2026-08-18)** |

---

## XP-1 — Enforce append-only with a wire witness (P0)

**Why first.** Phase 2 of this document says the append-only diff check is the
one validator item never built, and calls it "the invariant every
compatibility guarantee in this document rests on … still unenforced."
FastFHIR built exactly this and it works. `--validate` currently checks only
*within* one revision: it cannot see a 1.0 field being moved, resized or
retyped, because that is a property of the **diff** between revisions.

Today's session is the argument. The `ATTRIBUTE_SIZE` entry went from six
bytes to seven deliberately, and nothing in the toolchain objected — the only
thing that noticed was a hosted fixture failing to load, hours later. A
witness would have said so at `--validate` time.

**Read first**
- FastFHIR's `tests/generator/wire_witness.py` — the whole file. Its
  docstring states the principle: the gate is wire stability, not C++ source
  text identity.
- FastFHIR's `tests/generator/test_wire_format.py` — how `_check_permanence`
  is used, and how a baseline is stored and refreshed.
- `generator/validate.py` — where the new check joins the existing five.
- This document's Phase 2, validator item 1.

### Locate

```bash
# from the repository root
grep -n "def validate" generator/validate.py | head
ls tests/wire/ 2>/dev/null || echo "no tests/wire yet (expected)"
python3 -m generator --validate | tail -1
```

**Expect:** `--validate` prints `generator: spec documents are consistent`,
and `tests/wire/` does not exist.

### XP-1.1 — Emit the witness — DONE

Add `generator/witness.py` with one entry point:

```python
def witness(fields_doc, constants_doc, header_doc) -> dict
```

It returns **only what reaches the wire**, never C++ text:

- `blocks` → for each block: `recovery_tag` value, `primitive`, and for each
  version group the ordered list of `(field name, type or enum group, width,
  offset)` derived through `generator.model.layout.derive_layout`.
- `entries` → the same for every array block's entry, plus `entry_size` per
  version group.
- `enums` → every group's `underlying_type` and every member's name → value.
- `constants` → every statically defined value's name → value.

Derive offsets through `derive_layout`, never by re-deriving them here: a
second derivation is a second thing to be wrong.

**Done when:** `python3 -c "import json,generator.witness as w, ..."` prints a
dict containing `ATTRIBUTE_SIZES.entry.KIND` at offset 6, width 1.

### XP-1.2 — Commit the baseline — DONE (file written; commit pending — see status below)

Write the witness for the current tree to `tests/wire/witness.json`,
**committed** (unlike `generated_source/`, this is evidence, not output).
Add a one-paragraph `tests/wire/README.md` stating: what the file is, that it
is append-only evidence rather than a snapshot to refresh casually, and the
exact command to refresh it deliberately.

**Done when:** `tests/wire/witness.json` is committed and
`git diff --stat` is clean after regenerating it.

### XP-1.3 — Gate on it — DONE

Add check 6 to `generator/validate.py`, run by `--validate`: recompute the
witness and compare against `tests/wire/witness.json`. Report, per finding:

- a field that **moved, changed width, or changed type** → error
- a field or block that **disappeared** → error
- an enum member whose **value changed** → error
- anything **added** → accepted silently (that is the whole point)

Adopt FastFHIR's `_check_permanence` shape rather than writing a new
comparator — same rule, same wording, one fewer thing to disagree.

**Done when:**
```bash
python3 -m generator --validate && echo GATE-GREEN
```
exits 0, and each of these red-greens (revert after each):
- move a 1.0 field within a block → error names the field and both offsets
- widen `KEY_SIZE` u16→u32 → error names the width change
- append a new field to a `1.1` group → **passes**

⚠ **XP-1.4 — the baseline's own history — DONE.** `witness.json` records today's
tree, which already contains the deliberate 1.0 `ATTRIBUTE_SIZE` correction.
The gate therefore starts from a state that itself broke append-only once.
Record that in `tests/wire/README.md` in one sentence, with the commit hash,
so a future reader does not conclude the invariant has held unbroken. Do not
try to reconstruct a pre-correction baseline — the correction was deliberate
and its record is `tests/wire/README.md`. (It was also carried as an
erratum on the 1.1 revision in `spec/ife_header.json`; that was removed
2026-08-18 by decision — the correction belongs to 1.0, not to 1.1, and the
witness README is where a reader looking for the one append-only break will
be.)

> **Status (2026-08-18): XP-1 is DONE — XP-1.1 through XP-1.4.**
> `generator/witness.py` derives the wire witness (blocks, entries, enums,
> constants — only what reaches the stream, never C++ text) through
> `generator.model.layout.derive_layout`; the baseline is committed evidence
> at `tests/wire/witness.json` with `tests/wire/README.md` (which also
> records the baseline's own history — the deliberate 1.0 `ATTRIBUTE_SIZE`
> correction, commit `9e591fa`). Check 7 in `generator/validate.py` is the
> gate: a shipped fact that moves, widens, retypes or disappears errors;
> additions pass. Two extensions landed with it: `tools/refresh_witness.py`
> refuses to record a wire break and prints what it records; `--validate`
> warns (non-fatally) when the baseline lags the spec and `--check` fails
> until the deliberate refresh lands — so a version bump carries its
> baseline refresh in the same change. The baseline file is written but not
> yet committed (work-order rule: leave changes in the working tree).

---

## XP-2 — Rebuild `--check` on parity, not byte-equality (P1)

**Why.** Phase 3 already describes this: the current `--check` is exact
character equivalence, so it flags every legitimate change — a version bump, a
copyright year — as drift, and it still cannot answer "was this produced from
the current spec?" XP-1 supplies the answer.

**Depends on XP-1.**

### XP-2.1 — ✅ DONE
Embed the witness hash in each generated file's banner
(`generator/emit/cpp.py::_banner`, `generator/emit/docs.py::_banner`).

### XP-2.2 — ✅ DONE
Change `--check` to compare the on-disk banner hash against a freshly computed
witness hash, and drop the full-text diff.

**Done when:** touching a comment in `generator/emit/cpp.py` and regenerating
leaves `--check` green, while changing any field in `spec/ife_fields.json`
turns it red. Both red-greened.

> **Status (2026-08-18): XP-2 is DONE — XP-2.1 and XP-2.2.**
> `witness_hash()` in `generator/witness.py` fingerprints the canonical
> witness; every generated banner (C++ `//` and AsciiDoc `////`, both via
> `_banner`) carries `Wire witness: sha256 <hex>`. `--check` is now parity,
> not byte-equality: it extracts each on-disk banner's hash and compares it
> against a fresh computation — a comment edit in an emitter no longer flags
> as drift, a file produced from a different spec does. The full-text diff is
> dropped; missing files, orphans, and the stale-evidence promotion from XP-1
> remain. One banner-less file is exempt by design: `attributes.adoc` is the
> AsciiDoc document header and cannot carry a comment block. Red-greens
> verified: emitter comment change + regenerate → `--check` green with the
> hash unchanged; spec field change → `--check` red (both via the stale
> evidence promotion and, with the baseline refreshed, via banner-hash
> mismatch naming every drifted file).

---

## XP-3 — Port the determinism test (P2) — ✅ DONE

FastFHIR has `tests/generator/test_determinism.py`; this repository asserts
byte-stable regeneration in prose only. Add `tests/generator/test_determinism.py`
running the generator twice into two temp dirs and comparing.

**Done when:** the test passes, and passes again with `PYTHONHASHSEED=0` and
`PYTHONHASHSEED=1` — the seeds are the point, since dict ordering is where
non-determinism enters.

> **Status (2026-08-18): XP-3 is DONE.** `tests/generator/test_determinism.py`
> runs the generator twice into separate trees and compares every emitted
> file with `filecmp` — stdlib-only, like the generator, and collectable by
> pytest should one join the toolchain. Wired into ctest as
> `ife_generator_determinism` and into CI under `PYTHONHASHSEED=0` and `=1`
> (the seeds propagate to the generator subprocess). Verified green under
> all three environments — the emitters iterate insertion-ordered dicts,
> never hash-ordered sets, so no non-determinism was found to fix.

---

## XP-4 — Port the dangling-view test (P2) — ✅ DONE

FastFHIR has `tests/generator/test_no_dangling_views.py`. This repository has
the same hazard and no test: `IFE::ByteSpan` and every accessor returning one
point into a caller-owned mapping, and `Abstraction::AttributeNode::value`
copies while `bytes()` does not.

Add `tests/ife_lifetime_tests.cpp` asserting, under ASan, that no API returns a
view outliving the buffer it was built from. **Contract correction
(2026-08-18): the abstraction reads over a lens of the caller's mapping, as
FastFHIR reads over its arena — the caller keeps the mapping (VMA) alive, and
views into it are the design, not a hazard. The hazard the test guards is a
view bound to a *temporary*, which dies before the tree is walked.**

**Done when:** the test passes under `-fsanitize=address` and fails when a
`std::string` copy in `IFE_Runtime.cpp` is deliberately changed to a view.

> **Status (2026-08-18): XP-4 is DONE.** `tests/ife_lifetime_tests.cpp` builds
> the abstraction over the corpus snapshot's heap mapping, keeps the mapping
> alive (the lens contract), and walks every string the tree keeps with value
> checks against `v1_fixture::expectations()`. Passes in the normal build and
> under `-fsanitize=address,undefined` (14/14 ctest). Red-greens under the
> corrected contract: changing `AttributeNode::value` to a view *into the
> mapping* passes — views are the design; changing it to a view *bound to a
> temporary* fails, via corrupted values caught by the value checks — exactly
> the FastFHIR symptom class ("the store pass read freed memory… produced
> non-UTF-8 bytes") — because the temporary's freed block is reused rather
> than left poisoned. Both mutations reverted; the runtime files are pristine.

---

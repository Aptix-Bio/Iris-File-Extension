# Recovery work orders RC-1 to RC-10: two-witness reconciliation and gap analysis

> **Record.** Written 2026-08-28 to 2026-09-09 in MIGRATION.md at the repository root; moved here on
> 2026-11-24 when `development/` replaced the root planning documents (git history
> keeps the originals; the last commit that holds them is 005347e). Kept as written,
> with local paths, sibling-checkout paths and a quotation restated for a public
> repository. Open items are tracked in [WBS.md](../WBS.md) under the same IDs; their
> boxes here read `(open: WBS.md)`. Dated records keep the names they had (Iris-Codec
> is now Iris-Slide).


Written 2026-08-28 after reading the last three FastFHIR commits
(FastFHIR — all three are about recovery) and this repository's existing
scan-only `recover_file_structure`. The spec has designed the redundancy since
1.0 — every block carries the universal header {VALIDATION == own offset, a
RECOVERY tag sharing the 0x55 high byte}, and tile streams may carry a 40-bit
self-offset frame — and the reader has always had half the machinery:
`recover_file_structure` *finds* blocks; it never *reconciles a reference*.
FastFHIR just built the missing half, twice: the two-witness reconciliation
(P0-3 / REC-10…17) and the gap analysis that finds the block with **no
surviving witness at all** (REC-18). IFE's redundancy is the same design — its
`FileMap` is literally the model FastFHIR's `StreamMap` was written against —
so this port is structural, not inventive.

**Why now — the three FastFHIR commits, in order:**
- `67f4d83` (2026-08-27) — archive recovery; in the same pass fixed two honesty
  gaps (deserialize dropped 736 fields across 37 generated files; round-trip
  "measured nothing" because `diff_doms({}, {})` is zero diffs).
- `1c98659` (2026-08-27) — the REC-18 gap-analysis plan.
- `6a56c2b` (2026-08-28) — the REC-18 implementation: extents for every map
  entry, the containment rule, the Gap sweep, classification, the report
  surface. Measured on clean Synthea streams: 60,664 / 67,057 / 65,414 entries,
  **0 gaps, 0 overlaps**; eleven tests red-before/green-after; disabling the
  extent model reddens 12 assertions across 6 suites. Two of the tests pin
  that a hole names the RIGHT bytes, not merely that one appeared —
  `holes_locate_and_size_every_entry_shape` and
  `broken_blockref_still_locates_and_sizes_the_orphan` (the double-hit case:
  both the parent reference AND the child VALIDATION destroyed — the highest
  level of syntactic corruption, which destroys the redundancy itself).

**The property this ports (FastFHIR P0-3).** One logical edge A→B (parent
block A holds an offset to child block B) is stored twice, and a single
corruption cannot reach both halves:

| # | Witness | Where it lives in IFE | What it asserts |
|---|---|---|---|
| 1a | parent slot identity | the slot's fixed offset inside A (e.g. `TILE_TABLE::offset::CIPHER_OFFSET`) | *which* field of A this edge is — compile-time |
| 1b | parent target | the u64 in that slot | *where* B is (absolute offset) |
| 1c | parent expectation | the slot's compile-time child type (CIPHER_OFFSET → CIPHER, …) | *what* B must be — compiled in, uncorruptible |
| 2a | child self-offset | B's VALIDATION (+0, u64) | where B thinks it is |
| 2b | child identity | B's RECOVERY (+8, u16 0x55xx) | what B is |

`validate_file_structure` reads all of these and uses them only to *reject* —
at the moment it reports BAD_VALIDATION it is holding everything needed to
repair the edge, and it throws that away. These orders stop discarding it.

**The three damage cases** (REC-18's table, IFE's names):

| damage | scan sees it? | graph walk reaches it? | verdict |
|---|---|---|---|
| parent slot flipped, child intact | yes | no | orphan — Corroborated (mode 1) |
| child VALIDATION flipped, parent intact | **no** | yes (parent names it) | PositionRepaired |
| **child VALIDATION flipped AND parent slot broken** | **no** | **no** | **invisible — a Hole; absence is the only evidence** |

The third row is the orphaning case the tests must pin: a block with *neither*
witness left is not a pointer problem, it is a *bytes* problem — every other
block's extent is known, so the orphaned run is a hole, located and sized. A
hole says *something of size N was here*, not what; it turns "invisible" into
"located and sized", which is what a repair ranker needs to have a candidate
position at all.

**Read first**
- FastFHIR's `include/FF_Recovery.hpp` — the whole port in one header:
  StreamMap (one map type, two producers), BlockRef, RepairClass, the
  FF_RecoveryReport, Recovery, and the REC-17 threat-model boundary.
- FastFHIR's `src/FF_Recovery.cpp` — scan, reconciliation, `find_gaps`.
- FastFHIR's `tests/cpp/test_recovery.cpp` — every test below in its original
  form, including the two-witness orphaning tests.
- FastFHIR's `TASKS.md` §P0-3 and §REC-18 — the governing principle and the
  gap-analysis rationale. (REC-10…17 are deleted-as-done from TASKS.md;
  `git log -S'REC-10' -- TASKS.md` finds the record.)
- `src/IFE_Runtime.cpp` §Recovery — what IFE has today (scan-only, size 0
  entries, no reconciliation).
- `spec/ife_spec.adoc` §Universal Block Header, §Tile Frame, §Recovery Codes —
  the redundancy the spec already promises.

**Rules: the cross-pollination rules at §CROSS-POLLINATION WORK ORDERS apply
unchanged** — one task ID per session, run *Locate* first and STOP on a
mismatch, do not commit, never hand-edit `generated_source/` or
`generated_docs/`, append-only is inviolable, and ⚠ marks a decision a flash
model must not take alone. One rule this section adds: **every repair is
reported, never silent** — a repaired file is not the same object as an intact
one, and `apply()` is the only path that mutates.

## Priority summary

| ID | Priority | Task | Why | Status |
|---|---|---|---|---|
| RC-1 | **P0** | Two-witness reconciliation | `scan()` finds blocks; nobody reconciles a reference | ✅ engine + tests |
| RC-2 | **P0** | Extent model + gap analysis | a block with BOTH witnesses destroyed is otherwise invisible | ✅ sweep + RC-2.4 |
| RC-3 | **P0** | Orphaning tests | the two-corruption case is the one the current tests cannot express | ✅ written, green (RC-3.1…3.5) |
| RC-4 | P1 | Report surface, wiring, boundary doc | repairs must be countable, wired, and honestly bounded | report + boundary ✅; wiring open |
| RC-5 | P2 | Checksum footer | damage *detection* with no second witness — proves something changed, localizes nothing; the RC-1/RC-2 localizers need a detector to point at them | open |

---

## RC-1 — Two-witness reconciliation (P0)

**Why.** `recover_file_structure` is a census, not a repair: it records every
self-consistent block and returns nothing about *references*. FastFHIR's
`recover()` reconciles every parent→child edge by whichever witness survives —
child VALIDATION broken, parent names it (PositionRepaired); parent slot
broken, a unique matching orphan corroborates it (Corroborated); child tag
broken, the slot's compiled expectation rewrites it (TagRepaired); array
extent broken, recomputed (ExtentDerived) — and reports a verdict per
reference with the Hamming bit cost of the fix, never guessing: ≥2 live
readings at equal cost is `Ambiguous`, nothing within budget is `Unrecovered`.

**Status (2026-08-28).** The engine ships and is green under CMake — 17/17
ctest including the RC-3 suite (generator `--validate`/`--check`; portability
lint): the 18-edge walkers (RC-1.1), the scan+walk join with the orphan census
(RC-1.2), the Hamming ranker (RC-1.3) and `apply()` (RC-1.4) live in
`src/IFE_Recovery.cpp` / `include/IFE_Recovery.hpp`, and
`tests/ife_recovery_tests.cpp` now executes every path — walkers, `recover()`,
`classify()`, `hamming_cost()`, `apply()` and the gap sweep — on real corpus
bytes (RC-3.1…3.5). Four engine defects found and fixed while writing those
tests: `scan()` now notes the validated FILE_HEADER (its run previously read
as a Hole on every clean file); `walk_attributes()` now slices the packed run
by key+value per the runtime's contract (it had read nested offsets from key
text); the entry-based walkers now store RELATIVE slots so `apply()`'s
Corroborated write lands (it wrote at parent+absolute); `apply()` writes the
tile slot as a u40 (a u64 write clobbered the adjacent u24 SIZE); and
`classify()`'s tile branch now consults frame-witnessed orphans (the
frame-survival rebuild was dead code), reporting Unrecovered when frame
witnesses exist but none is within budget, and ranking them by hamming
distance AND size — the entry's claimed extent must end at a claim boundary.
Bazel is fixed and green: 16/16 `bazel test //...` (the missing declarations
were `src/IFE_Common.hpp` in the `ife` library AND the example-runtime
target, plus `src/IFE_Recovery.cpp` there — the earlier "skipped" reports
were the transitive build failure, not environmental; the two
`src/IFE_Common.hpp` declarations were removed again when the file was
folded into `IFE_Primitives.hpp`).

**Review pass (2026-08-28, same day).** A review of the branch plus an
exhaustive damage sweep found four more defects, all now fixed and pinned;
RC-2.4 and the RC-2.3 size floor landed in the same pass, so no sub-item is
knowingly unimplemented any more.

1. **Bounds checks that wrapped.** `__off + n <= __size` on wire offsets is
   not a bounds check: NULL_OFFSET is all-ones and is what every unused slot
   holds, so ONE flipped bit gives 0xFF…FE, `0xFF…FE + 10` wraps to 8, and
   the engine read eight bytes from *before* the mapping (ASan; on an mmap'd
   slide the base is page-aligned, so that is the previous page). Every guard
   in the file now goes through `in_range()`, which subtracts.
2. **`apply()` was not all-or-nothing.** It wrote as it went and refused
   afterwards, leaving a half-repaired file — the one state its own contract
   forbids. It is now plan-then-commit: the whole report is range-checked
   before a byte is touched (`write_repair(..., commit=false)`).
3. **`scan()` recorded entries past EOF.** Extents come from wire fields, so a
   damaged ARRAY COUNT — or a tile entry naming an arbitrary 40-bit address —
   put map entries, and the gap runs after them, outside the file. `claimable()`
   now clamps; an OFFSET outside the file is not recorded at all. The overrun
   is not lost: reporting it is `ExtentDerived`'s job.
4. **The tile branch's frame subtraction underflowed.** `target - 5` on a
   target below 5 gives all-ones — exactly the value an ABSENT witness holds —
   so an entry corrupted to 4 was reported **Intact**. Red-green confirmed.

Also this pass: `tests/ife_damage_sweep_tests.cpp` (the exhaustive sweep, new
ctest + Bazel target, 14,475 single-site damages, clean under ASan+UBSan) and
three regressions in the RC-3 suite. A `__out` parameter in the RC-3 helper
was tripping the portability lint's SAL check; renamed.

### Locate

```bash
# from the repository root
grep -n "recover_file_structure" src/IFE_Runtime.cpp     # scan-only today
grep -n "enum class RepairClass" FastFHIR's include/FF_Recovery.hpp
grep -n "FF_RECOVERY_MAX_FLIPS" FastFHIR's include/FF_Recovery.hpp   # 8
```

**Expect:** IFE has one producer (the scan); FastFHIR has seven repair classes
and a flip budget of 8.

- [x] **RC-1.1 — Reference enumeration: the IFE edge inventory.** The
      FastFHIR `BlockRef{parent, field, child, stored, expected}` needs an IFE
      producer. The edges are the 18 parent→child references the spec defines;
      the expected child tag of each slot is compile-time (1c). Enumerate from
      the generated layer — slot offsets are `b::<Parent>::offset::<SLOT>` —
      never a second literal table. The full inventory, which is also the
      Locate ground truth for every test below:

      | Parent | Slot / entry | Child | Notes |
      |---|---|---|---|
      | FILE_HEADER | `TILE_TABLE_OFFSET` | TILE_TABLE | required |
      | FILE_HEADER | `METADATA_OFFSET` | METADATA | required |
      | TILE_TABLE | `CIPHER_OFFSET` | CIPHER | nullable; NULL_OFFSET in every fixture to date |
      | TILE_TABLE | `LAYER_EXTENTS_OFFSET` | LAYER_EXTENTS | array |
      | TILE_TABLE | `TILE_OFFSETS_OFFSET` | TILE_OFFSETS | array |
      | TILE_OFFSETS | `entry[i]` {OFFSET, SIZE} | TILE_PIXEL_DATA | stream; the entry is the stream's only extent witness; the optional TILE_FRAME is its second witness (index, not length) |
      | METADATA | `ATTRIBUTES_OFFSET` | ATTRIBUTES | nullable |
      | METADATA | `IMAGES_OFFSET` | IMAGES | array |
      | METADATA | `ICC_COLOR_OFFSET` | ICC_PROFILE | byte array |
      | METADATA | `ANNOTATIONS_OFFSET` | ANNOTATIONS | array |
      | METADATA | `CLINICAL_OFFSET` | CLINICAL_METADATA | byte array |
      | ATTRIBUTES | `SIZES_OFFSET` | ATTRIBUTE_SIZES | array |
      | ATTRIBUTES | `BYTES_OFFSET` | ATTRIBUTE_BYTES | byte array |
      | ATTRIBUTE_SIZES | `entry[i]` nested, KIND == ATTRIBUTE_NESTED | child ATTRIBUTES | the nested triple is three ordinary blocks |
      | IMAGES | `entry[i]` BYTES_OFFSET | IMAGE_BYTES | |
      | ANNOTATIONS | `entry[i]` BYTES_OFFSET | ANNOTATION_BYTES | |
      | ANNOTATIONS | `GROUP_SIZES_OFFSET` | ANNOTATION_GROUP_SIZES | array |
      | ANNOTATIONS | `GROUP_BYTES_OFFSET` | ANNOTATION_GROUP_BYTES | byte array |

      **Done when:** on a clean `v1_slide()` every reference the fixture
      actually encodes reports `Intact` at `bit_cost 0`, legal NULL slots
      report *absence, not damage* (they must not fabricate references), and
      the enumeration reuses the generated slot offsets — a new block type is
      a compile error, not a silent gap.
- [x] **RC-1.2 — Join with the scan: orphan test + classification.** Two
      producers, one map type (the FastFHIR shape): `generate_file_map`'s walk
      is the clean baseline; `recover_file_structure`'s scan is the recovery
      producer — the census of *self-consistent* blocks. `recover()` runs both
      and joins: an edge whose parent slot and child header disagree is
      classified per the repair classes above; a self-consistent block no
      intact slot names is an orphan (corroboration candidate). Scan entries
      must carry the RC-2 extent model so the join can size orphans.

      **Done when:** a 1-bit VALIDATION flip reports `PositionRepaired` at
      `bit_cost 1`; a 1-bit parent-slot flip reports `Corroborated` at
      `bit_cost 1`; a 1-bit tag flip reports `TagRepaired`; and on a clean
      stream every verdict is `Intact` with zero fabricated references — the
      FastFHIR D3/D4/D5 regression: inline scalar values, packed values and
      legal null array entries must not invent edges or extents.
- [x] **RC-1.3 — The Hamming ranker.** `popcount`-based, `constexpr`, budget 8
      (FastFHIR's `FF_RECOVERY_MAX_FLIPS`): a candidate whose repair costs
      more than 8 flips is not a hypothesis, it is a guess. `Ambiguous` when
      ≥2 candidates tie within budget — reported with its candidates, never
      picked; `Unrecovered` otherwise. Candidates come from (a) the orphan set
      (mode 1) and (b) RC-2.4's holes — a hole of size N adjacent to an
      unsatisfied slot is a position candidate.

      **Done when:** FastFHIR's REC-12 unit cases (ported) pass, including the
      equal-cost tie reporting `Ambiguous`, never a silent pick.
- [x] **RC-1.4 — `apply()` — the only mutating path.** Takes the report,
      writes back only the repairs it classes: the parent slot
      (Corroborated), the child VALIDATION (PositionRepaired), the child
      RECOVERY tag (TagRepaired), the array extent (ExtentDerived). Rebuilding
      a tile-offsets entry from a surviving frame — the existing scan already
      reads `TILE_INDEX`; move it from scan into this path. `recover_file_structure`
      itself stays read-only, exactly as `FileMap` is today.

      **Done when:** `apply()` is the only function in the repository that
      writes recovered bytes; a report with zero repairs mutates nothing
      (byte-identical buffer).

---

## RC-2 — Extent model + gap analysis: the block with no witness (P0)

**Why.** The two-corruption case (child VALIDATION flipped AND parent slot
broken) is invisible to scan and walk alike — the damage table above. It can
only be found by **absence**: if every other block's extent is known, the
bytes nothing claims are the lost block, located and sized. FastFHIR measured
the arena tiling exact on 60k-entry streams (0 gaps, 0 overlaps once the
containment rule landed; the single residual 352-byte gap was a header-only
charge on the URL directory's entry payload — the rule that fixed it is
RC-2.2). IFE's `generate_file_map` already computes most extents; the work is
applying the same formulas to scan-found entries and adding the sweep.

### Locate

```bash
# from the repository root
grep -n "array_span\|blob_span" src/IFE_Runtime.cpp          # extent formulas, walk side
grep -n "note(map, .*0)" src/IFE_Runtime.cpp                 # scan records size 0 today
grep -n "header_size_v1_0\|header_size_v1_1" generated_source/IFE_Blocks.hpp   # version-skew source
```

**Expect:** the walk sizes everything, the scan sizes nothing, and two
generated header sizes differ between 1.0 and 1.1 (TILE_TABLE 44→46, METADATA
56→68) — the version-skew source.

- [x] **RC-2.1 — Extents for every entry.** Scan-found blocks sized from their
      own bytes, same formulas the walk uses: plain blocks `header_size`;
      arrays `header_size + COUNT × STRIDE` (`array_span`); byte arrays
      `header_size + COUNT` (`blob_span`); IMAGE_BYTES
      `header_size + title_size + image_size`; TILE_FRAME
      `TILE_PIXEL_DATA::header_size`. TILE_PIXEL_DATA is the one exception: a
      stream has no header, its extent lives only in the tile-offsets entry
      that addresses it — so in the scan, tile-stream bytes are sized *by the
      entry* when the entry survives and read as a hole when it does not. That
      is honest: a stream without its entry has no derivable length, and the
      frame does not carry one.

      **Done when:** the FastFHIR feasibility probe, ported as a test: on the
      v1 fixture (every shape present), `Σ extent == file_size`, 0 gaps, 0
      overlaps, 0 entries with no derivable size. If a gap shows up, it is an
      extent-formula or fixture defect — find and fix it the way FastFHIR
      found the URL directory entry table, and pin it with a test.
- [x] **RC-2.2 — The containment rule.** Nothing double-counts. Inline arrays
      (LAYER_EXTENTS, TILE_OFFSETS, ATTRIBUTE_SIZES, IMAGES, ANNOTATIONS,
      ANNOTATION_GROUP_SIZES) are charged `header + COUNT × STRIDE`; the
      blocks their entries *name* (IMAGE_BYTES, ANNOTATION_BYTES, nested
      ATTRIBUTES triples) are separate entries charged separately. The frame
      is a separate block with its own VALIDATION, charged its own size; the
      stream it precedes is charged to the entry, not to the frame.

      **Done when:** the tiling probe reports 0 overlaps; a double-count bug
      in any of the six array shapes turns the probe red.
- [x] **RC-2.3 — The sweep + classification.** One O(n) pass over the
      offset-ordered map: every run `[cursor, next_offset)` nothing claims is
      a gap. Classify, cheapest discriminator first:
      1. **Version gate** — a gap can only be benign if a NEWER engine wrote
         the file: `FILE_HEADER.EXTENSION_MAJOR/MINOR`. IFE is lucky here: the
         1.1 block expansion (TILE_TABLE 44→46, METADATA 56→68) is the exact
         skew this class exists for, and there are real 1.0/1.1 fixtures to
         test against — FastFHIR's systematicity half is still untestable
         there for lack of a genuinely newer stream.
      2. **Systematicity** — a version delta trails *every* instance of a tag
         at the same size; a hole is a one-off. Self-calibrating — the reader
         cannot know the newer layout, but it can observe 84 tiles all trailing
         the same run and conclude the delta rather than 84 holes.
      3. **Size floor** — a hole must be ≥ the smallest block header. Never
         decides alone.
      `Hole` (damage), `VersionSkew` (benign), `Trailing` (after the last
      entry). A same-version stream must **never** report skew — getting that
      backwards reclassifies real damage as benign, which is strictly worse
      than reporting nothing.

      **Done when:** flipping one byte of a mid-file block opens exactly one
      Hole at the right offset and size; a 1.0-sized reader over a 1.1 file
      reports VersionSkew, never Hole; and the same-version-never-skew
      assertion is red before the version gate exists.

      **Status (2026-08-28):** `find_gaps()` ships — version gate
      (`declared > VERSION_WRITTEN`), systematicity trail (2+ instances),
      Trailing — but the size-floor sub-rule is not enforced (any
      unattributed run, however small, classes as Hole). The probe clauses
      above are now the green RC-3.1 + version-skew tests.
- [x] **RC-2.4 — Feed the ranker.** A hole of size N adjacent to an
      unsatisfied parent slot is a position candidate for that slot's child.
      (FastFHIR marked REC-18.6 done and then corrected it in the 6a56c2b
      message as still unbuilt — build it in from the start here.)

      **Done when:** a two-witness-destroyed block is both reported as a Hole
      (RC-2.3) and offered as a ranked candidate to RC-1.3, and `apply()`
      restores the parent slot to it when the cost is within budget.

      **Status (2026-08-28): implemented.** `classify()`'s both-witnesses-gone
      branch ranks holes when — and only when — no surviving orphan is within
      budget. **An orphan outranks a hole categorically**, never on bit
      distance: an orphan is a block saying "I am here", a hole is only the
      absence of one, and ranking them in one pool would let destroyed bytes
      outbid an intact block on a closer bit pattern. RC-2.3's size floor is
      load-bearing here and is now applied (a run smaller than the universal
      block header is noise, not a position).

      The verdict is its own class, **`HoleCorroborated`**, counted apart in
      `RecoveryReport::hole_corroborated` — ⚠ this was a naming/semantics call
      taken while implementing: reusing `Corroborated` would have conflated a
      pointer restored to a *surviving* block with a pointer restored to
      *destroyed bytes*, and the second is what this class is. `apply()`
      writes the parent slot for both.

      The repairs chain, which `test_hole_is_a_position_candidate` pins
      end-to-end on cipher_iris: destroy a child's VALIDATION and one bit of
      the parent slot → the block vanishes from the census, one Hole is
      reported at its exact offset and extent, the edge is HoleCorroborated at
      cost 1 → `apply()` restores the pointer → the second pass classes the
      same edge PositionRepaired (the tag survived, so the parent-named
      address recomputes VALIDATION) → `apply()` again, and the file re-tiles
      with no gaps and every edge Intact.

---

## RC-3 — Orphaning tests (P0)

**Why.** The current recovery tests damage one side and assert the scan still
finds things (`test_recovery_finds_blocks_without_the_offset_graph`,
`test_recovery_finds_tile_frames_and_rebuilds_entries`). The case the
format's redundancy exists for — **a data block with neither witness left** —
has no test, because the current API cannot express it. FastFHIR's
`test_recovery.cpp` is the pattern: writer-produced fixtures, one damage per
known edge, the repair asserted *and* reported with its class and bit cost,
and a **non-zero lower bound on edges damaged** asserted before any recovery
rate — a harness that corrupts nothing recovers everything (P0-2).

### Locate

```bash
# from the repository root
grep -n "recover" tests/ife_runtime_tests.cpp     # scan-only tests today
ls tests/ife_recovery_tests.cpp 2>/dev/null || echo "no recovery tests yet (expected)"
grep -n "holes_locate_and_size_every_entry_shape" FastFHIR's tests/cpp/test_recovery.cpp
grep -n "broken_blockref_still_locates_and_sizes_the_orphan" FastFHIR's tests/cpp/test_recovery.cpp
```

**Expect:** IFE tests flip one byte; FastFHIR has a hole-location test and a
both-witness-destroyed test to port. New file `tests/ife_recovery_tests.cpp`,
built on the in-memory `v1_slide()` writer fixture (writer output, not
hand-built buffers — the COV-1 reason).

**Status (2026-08-28): written and green.** `tests/ife_recovery_tests.cpp`
covers RC-3.1…3.5 (CMake 18/18 + Bazel 16/16; wiring: CMake target in
tests/tests.cmake, Bazel cc_test in tests/tests.bzl). Fixtures: cipher_iris +
v1_1 (clean baselines, 0 gaps), v1_0 (its frozen CLINICAL slot defect is
pinned — the engine reports it exactly once, never silently — and its nested
triple is the RC-3.3 nested victim). Zero-coverage paths closed with the
suite: a damaged root (Hole at [0, root_extent), census-only recovery), the
unframed tile-entry corruption (Intact + exact Hole at the stream), the
VersionSkew positive direction (4-instance systematic delta, never Hole on a
same-version stream), the exact ExtentDerived recompute, apply()'s
out-of-range aborts, and the tiny-file scan. The engine fixes that landed
with these tests are recorded in the RC-1 status block above.

- [x] **RC-3.1 — Clean-stream zero-damage regression.** The full v1 fixture
      walked by the new reconciliation: every encoded edge `Intact`, zero
      holes, zero gaps, and — the regression that matters — **no fabricated
      references**: legal NULL_OFFSET slots, packed attribute values, empty
      arrays and the nested attribute chain must not invent edges or extents.

      **Done when:** the clean fixture reports `intact == blocks_total`,
      `holes == 0`, `gaps == 0`, and a reference-inventing bug in RC-1.2 turns
      the test red.
- [x] **RC-3.2 — One witness per class.** For each repair class: damage
      **exactly one witness** of a known edge in the writer-produced fixture
      and assert the edge is restored and reported with the expected class and
      bit cost — VALIDATION flip → PositionRepaired @ 1; parent slot flip →
      Corroborated @ 1; tag flip → TagRepaired; COUNT flip → ExtentDerived.
      Assert a non-zero lower bound on edges damaged before asserting the
      recovery rate: count the damages, prove ≥ 1 took.

      **Done when:** one test per class, each red before the RC-1 code and
      green after, each asserting class + bit cost, none passing on an
      undamaged stream.
- [x] **RC-3.3 — Both witnesses destroyed: the orphan, located and sized.**
      The centerpiece. Destroy a block's VALIDATION **and** its parent's
      reference — the two damage sites must be **distinct bytes**, because for
      some references the slot address and the child's own offset coincide and
      two `^= 0xFF` writes to one byte cancel, silently producing an
      undamaged stream and a green-looking test (FastFHIR prints skip counters
      exactly because that is how a test quietly stops testing anything).
      Assert the precondition the test stands on: after damage, the block is
      **not** in the scan — if it did not actually vanish, everything below
      passes for the wrong reason. Then assert a Hole exists, at the lost
      block's offset, sized to the lost block's extent. One victim per shape —
      plain block, array, byte array, and a nested attribute triple — because
      the extent rules differ per shape.

      **Done when:** `holes_locate_and_size_every_entry_shape` and
      `broken_blockref_still_locates_and_sizes_the_orphan`, ported and passing
      over the v1 fixture; disabling the extent model or the sweep reddens
      them.
- [x] **RC-3.4 — Frame survival rebuilds the tile entry.** The IFE-only case
      FastFHIR has no analog for: destroy a tile-offsets entry (the stream's
      only extent witness) while its frame survives — the frame's 40-bit
      VALIDATION and TILE_INDEX restore the entry's position and index.
      Extent stays the codec's question: the repair restores the *reference*,
      not the stream length.

      **Done when:** the entry is rebuilt from the surviving frame with the
      correct TILE_INDEX, reported as a repair, and a stream whose frame is
      also destroyed is reported as a Hole, not guessed.
- [x] **RC-3.5 — Never silent; red-green both ways.** Ambiguous ties reported
      with candidates, never picked; Unrecovered edges reported, never
      dropped; `apply()` on a zero-repair report mutates nothing. Then the
      honesty gates: re-introducing a bug into the reconciliation, the extent
      model, or the sweep turns a test red (FastFHIR: disabling the extent
      model reddened 12 assertions across 6 suites), and a damaged-but-untested
      edge count of zero fails the run.

      **Done when:** every damage the tests inflict is either
      repaired-and-reported or reported-unrecovered — nothing is both silent
      and wrong — and each RC-1/RC-2 feature has its red-green pair.

---

## RC-4 — Report surface, wiring, boundary doc (P1)

- [x] **RC-4.1 — The report.** FastFHIR's `FF_RecoveryReport` shape, beside
      `FileMap` in `include/IrisFileExtension.hpp`: per-class counts
      (`intact`, `corroborated`, `tag_repaired`, `position_repaired`,
      `extent_derived`, `ambiguous`, `unrecovered`), `blocks_total`,
      per-reference verdicts (parent, field, child, tag, class, bit_cost,
      candidates), and the gap list with `holes`/`version_skew` counts.
      `FileMap` gains the gap list the way `StreamMap` does. Counts
      precomputed so a driver reports recovery without re-walking.

      **Done when:** a driver prints "N blocks recovered / M total, H holes,
      V version skew" with no new walks.
- (open: WBS.md) **RC-4.2 — Wire it.** CMake target + ctest, a Bazel `cc_test`, and the
      corpus harness unions the recovery *report* into its walk the way it
      already unions the scan types — real corpus bytes are the only place the
      two-witness path is exercised at scale.

      **Done when:** `ctest` and `bazel test //...` are green with the new
      targets, and the corpus harness still passes with the union.

      **Status (2026-08-28): one clause left.** `ife_recovery_tests` is a
      CMake target and green under ctest, and a Bazel `cc_test` runs green
      under `bazel test //...` (16/16). What remains: the corpus harness
      does not yet union the recovery *report* (it already unions the scan
      types).
- [x] **RC-4.3 — The boundary, documented beside the code (REC-17).** The
      threat model and the no-second-witness list go in
      `include/IrisFileExtension.hpp` beside the declarations, so no consumer
      reads the redundancy as a guarantee of total recoverability. IFE's list:
      FILE_HEADER (magic, no VALIDATION — the root); inline attribute values
      packed in ATTRIBUTE_BYTES (no header); leaf payload bytes (ICC_PROFILE,
      CLINICAL_METADATA, IMAGE_BYTES, ANNOTATION_BYTES content — a repaired
      block header does not recover the payload); TILE_PIXEL_DATA streams
      (extent lives only in the entry; the frame carries index, not length);
      both witnesses damaged on the same reference (a Hole: located and sized,
      content gone); and **bit-flip only** — truncation, overwrite or memmove
      damage defeats the ranker and is reported Ambiguous/Unrecovered, never
      guessed.

      **Done when:** the boundary is stated in the header beside the API it
      bounds, and the RC-3 tests cite it.

      **Status (2026-08-28):** the threat model and no-second-witness list
      are documented in `include/IFE_Recovery.hpp` beside the API they bound
      (value types beside `FileMap` in `IrisFileExtension.hpp`). The RC-3
      tests exercise the boundary's no-second-witness cases — both-witness
      damage reports a Hole or Unrecovered, never a guess.
- [x] **RC-4.4 — Byte-level bit/offset machinery is TU-confined.** No bit or
      offset *operation* is reachable through the public include chain.
      Verified 2026-08-28: `IrisFileExtension.hpp` includes only std headers,
      Iris-Headers' two type headers and the generated `IFE_Blocks.hpp` /
      `IFE_Map.hpp` / `IFE_Window.hpp`; the generated `IFE_Blocks.hpp`
      includes only `IFE_Bytes.hpp` (the byte-I/O utility). The offset math —
      tag prefix, `compose_version`, header offsets, extent arithmetic — lives
      in `IFE_Primitives.hpp`, which `IrisFileExtension.hpp` and
      `IFE_Blocks.hpp` deliberately do NOT include: only the translation units
      that do the math do (`generated_source/IFE_Blocks.cpp`,
      `src/IFE_Runtime.cpp`, `src/IFE_Recovery.cpp`). The reader-policy
      helpers (root bootstrap, map recording) live at the foot of
      `IFE_Primitives.hpp` — `src/IFE_Common.hpp` was folded into it
      (2026-08-28) — and remain out of the public chain. The public
      surface carries the spec's compile-time offset
      *constants* (`offset::SLOT`, `header_size` — witnesses 1a/1c), never
      byte manipulation.

---

## RC-5 — Checksum footer (P2)

**Why.** IFE has no whole-file integrity witness: `validate_file_structure`
proves structure, the recovery localizers find damage once a scan or tiling
suspicion exists, but nothing says *the file changed* — the redundancy is
per-block, and a rewrite that preserves every VALIDATION word is invisible.
FastFHIR's `FF_CHECKSUM` (`RECOVER_FF_CHECKSUM`, ×1 per bundle) is the
reference: a footer that proves something changed and localizes nothing — the
detector that hands RC-1/RC-2 a place to look. (FastFHIR threat model,
FF_Recovery.hpp: "the checksum footer proves *something* changed; it
localizes nothing.")

- (open: WBS.md) **RC-5.1 — Spec + wire.** A new spec block (tag `RECOVER_CHECKSUM`, a
      BYTE_ARRAY or dedicated BLOCK primitive) appended at EOF; the checksum
      covers the file's bytes up to the footer. A WIRE change: spec JSON +
      witness refresh (`tools/refresh_witness.py`), new generated handle,
      `entry_for`/`recovery_for`/`extent()` regenerate from the spec
      automatically.
- (open: WBS.md) **RC-5.2 — Writer + reader.** The block writers stamp it on finalize;
      `validate_file_structure` verifies it as the last deep check. Old
      readers must tolerate the trailing block (gap analysis already classes
      post-last-entry runs as Trailing, benign).
- (open: WBS.md) **RC-5.3 — Version story.** 1.1 is ratified; the footer is 1.2 material
      (append-only, new tag, no shipped field touched).
- (open: WBS.md) **RC-5.4 — Tests.** Corpus fixtures + witness gain the footer; a
      one-byte flip anywhere in the file trips the footer while the
      structural validators may stay green — the exact case the per-block
      redundancy cannot see.

      **Done when:** a byte flipped in tile payload — where no VALIDATION
      word exists — fails validation via the footer alone, and a clean file
      passes.

---

## RC-6 — The FastFHIR REC-19 rewrite's lessons (P0) — ✅ DONE

**Filed 2026-08-31, implemented the same session.** The FastFHIR recovery
engine was rebuilt end-to-end under REC-19 after RC-1…RC-5 landed here
(handoff rev 2, FastFHIR's `handoff.md` §2; the rewrite itself was
uncommitted there at the time of this port — the lessons, not the code, were
ported). The measured defects that rewrite fixed existed in IFE's engine in
the same shapes, and this order closes them:

| FastFHIR defect | IFE symptom before RC-6 | RC-6 change |
|---|---|---|
| defects 2/5 — enumeration driven by the scan census alone; a VALIDATION-damaged block is absent from the census, so every reference FROM it is never enumerated, and the report shows zero failures | same: the walk's descent gate was `validate()`, so a child whose VALIDATION was the damaged half stopped the descent; its subtree was silently lost | walk driven by parent attestation; descent and census admission use FastFHIR's corroborated-tag rule (self-offset valid-or-within-budget AND the wire tag maps to the slot's declared type); a parent-attested block is admitted to the census map so its run stops tiling as a hole |
| defect 1 — one-surviving-witness descent | the walk demanded a fully intact child | descent continues while the child's self-offset is merely within the flip budget (with the tag corroboration above) |
| REC-19.2 — the loss must be visible | a lost subtree showed zero failures | `ProducerFailure{kind, at, expected, actual, why}` audit on `FileMap` and `RecoveryReport` (`ScanTagInvalid`, `VTableRecoveryMismatch`, `InvalidSelfRef`); every damaged ref the walk, the orphan pass, the reapply or the census sees is recorded |
| REC-19.7 — the reapply loop | a repaired block's subtree stayed unenumerated until apply() + re-scan | after classification, every Corroborated / TagRepaired / PositionRepaired repair is re-enumerated under the corrected type behind the `refs_are_coherent` gate (wrong-turn-2: a wrong repair yields children with no surviving witness — the whole batch is dropped, never believed); Corroborated follows the ranker's candidate, not the rejected slot value (`repaired_target`) |
| REC-19.4 — the census remainder | an orphaned parent's outgoing references were never enumerated | census blocks the walk never reached are enumerated under their own wire type |
| REC-19.5 — parallel census | single-threaded byte walk | the signature pass is chunked across hardware-concurrency threads (sequential below 1 MiB, chunks overlap by HEADER_SIZE−1, duplicates dropped at the audit) |
| REC-18.6 — sized evidence | any hole ≥ the header floor was a candidate for any type | a fixed-size expected type accepts only a hole whose length equals what it occupies at that position; variable-size types keep the floor |
| wrong-turn-2's coherence gate | the reapply's verification was absent | `refs_are_coherent` checks every non-null child has a surviving witness before a corrected-type batch is believed |

**New tests (RC-3.6):** `one_damaged_witness_costs_nothing` asserts the
whole-report shape for both header halves — a one-bit VALIDATION flip loses
no references, opens no hole, reports PositionRepaired, and is recorded on
the audit; a one-bit RECOVERY-tag flip is TagRepaired and its subtree is
re-enumerated by the reapply. The damage sweep (14,475 single-site damages)
stays green under ASan+UBSan, and the clean-stream invariants (RC-3.1) did
not move.

**Also fixed on the way (latent, pre-existing):** `versioned_root` read the
root's version fields unguarded on buffers too small to hold the header
(caught by the tiny-file scan test under ASan); the walkers' array element
loops were bounded by the stamped COUNT alone (a damaged COUNT drove entry
reads past the mapping once the walk could descend into repairable arrays).

---

## RC-9 — Misattribution: the REC-23 delta (P0) — ✅ DONE

**2026-09-09.** FastFHIR REC-23 (`f8fc0dc`) found that 11 of 206 `Corroborated`
repoints attached the **wrong** block at 512 flips — not fabrication and not
loss but *misattribution*, real data on the wrong parent, reading as perfectly
valid. In 10 of 11 the true child's own witness was destroyed too, so the scan
never listed it, some innocent orphan sat within budget, and a single candidate
made the uniqueness test trivially true. **Uniqueness is a property of the
search, not of the evidence** — and IFE's ranker had the identical semantics.

### The locality premise does not transfer, and the corpus cannot tell you that

REC-23.1 brackets a candidate between the nearest intact same-slot references
**in parent order** (FastFHIR measured 1471/1472 monotonic). **Measured false
for IFE.** Iris-Codec claims a tile's arena space with
`offset.fetch_add(entry.size)` *after* compression returns
(Iris-Codec's `src/IrisCodecEncoder.cpp:751`), on `hardware_concurrency()`
workers CAS-claiming one shared grid, so a stream's offset records **completion
order, not tile order**:

| threads | adjacent pairs ascending | correct repoints the bracket rejects |
|---:|---:|---:|
| 1 | 100.00% | 0.00% |
| 8 | 57.76% | 60.32% |
| 18 | 55.92% | **62.46%** |

Widening does not rescue it — rank displacement is p50=5 but MAX=778 at 18
threads, and the tail tracks scheduler stalls and the encoder's 500 MB
`resize_file` write lock, none of it on the wire.

**The corpus nearly confirmed the opposite:** the witnesses measure 87/87
monotonic because the single-threaded snapshot writers produce them, not the
encoder. On those same bytes the cross-parent families (`ATTRIBUTE_SIZES` and
`ATTRIBUTE_BYTES` across three sibling `ATTRIBUTES` parents) already violate
1 of 2 pairs. *Before porting a heuristic, ask which writer produced the layout
it assumes.*

- [x] **RC-9.1 — `TILE_INDEX` adjudication.** The frame's `TILE_INDEX` is an
      exact identity witness for the one edge with a real misattribution
      surface, and the engine already read it into `BlockRef::recovery` and
      never consulted it: `classify()`'s tile branch ranked on hamming distance
      and the claim boundary, both estimates of *where*, never statements of
      *which*. A candidate whose frame names a different tile is now dropped.
      **Narrowing only** — an unframed candidate is not evidence and survives,
      so the filter removes wrong answers and never admits one the ranker
      refused; emptying the pool yields `Unrecovered`, which is honest. **Exact
      match, never hamming**: adjacent indices differ by one bit, so a distance
      test over an identity re-admits the confusion it exists to remove. Read
      from the bytes, not from `BlockRef::recovery` — `RecoveryCodes` is a u16
      and narrows the u32, so two tiles 65,536 apart share a truncated index.
- [x] **RC-9.2 — Exclusivity (REC-23.2), demote-only.** Repoints are picked per
      reference and greedily, so two damaged slots could both take the cheapest
      surviving block of their type and both report `Corroborated`. A contended
      target now demotes every claimant to `Ambiguous`, carrying the contended
      offset. An **Intact** edge stakes a claim too — both its witnesses agree
      on the wire, where a repoint is one hypothesis about a damaged slot.
      Verified first that a clean file names every child exactly once: the sole
      corpus duplicate is the frozen v1.0 writer-defect slot (METADATA's 1.1
      `CLINICAL_OFFSET` read on a 1.0 file, holding a real `ATTRIBUTES`
      address), which classifies `Unrecovered` and stakes no claim.
- [x] **RC-9.3 — A bound the frame path was missing.** `signature_matches`
      guarantees only the five VALIDATION bytes are in the file, but
      `TILE_INDEX` sits four bytes *behind* them, so an anchor at 5..8 with a
      self-consistent u40 read before the mapping. The frame grows backward;
      every read behind the anchor owes its own bound.

**Tests.** `test_contended_repoint_is_never_confident` forces two same-typed
edges onto one child and asserts neither claims it and `apply()` writes
nothing. RC-3.5's tile case now asserts the stronger outcome — two entries
corrupted to one equidistant value both recover **correctly**, where the engine
previously abstained on both — and the ambiguity half it carried moved to a
file that contradicts itself (both frames claiming one index). Both changes
were confirmed red-green by neutering each in turn.

**Verified.** ctest 18/18; `bazel test //:ife_recovery_tests
//:ife_damage_sweep_tests` 2/2; both suites green under ASan+UBSan; sweep
accounting unmoved (14,475 single-site damages, 269 hole repairs, 0 refused);
clean baseline unchanged (108 references, 107 intact, 1 unrecovered, 0 holes) —
the control that catches an accounting change wearing an improvement's clothes.

**Also landed:** `ife_recovery_bench_tests` — the bits→percent recovery curve
(the FastFHIR-benchmark Test 5 shape) run as an assertion on both build
systems. It compares ANCHORED units so a child reattached elsewhere cannot pass
as recovered, hashes content separately from identity, and asserts one
contract: an edge whose child carries an identity witness never comes back
naming a different real block. Its first cut asserted zero invention and zero
content change and failed immediately — correctly, because most of what it was
counting is damage this format never claimed to survive. What the two fixtures
now show, side by side: a **framed** tile edge never misattaches at any damage
level, while the **unframed** 1.0 fixture misattaches from four bits up. That
comparison is the price of a frame, measured.

---

## RC-10 — Open recovery work (ordered by exposure)

Filed 2026-09-09 out of the RC-9 session. Each item states what to change,
where, and what evidence already exists — nothing here needs re-deriving.

- [x] **RC-10.1 — The read path checks what `validate()` checks.** Measured
      before changing anything, and the premise was worse than written. One
      flipped bit in an `ICC_PROFILE` COUNT — both witnesses intact, recovery
      correctly reporting 18/18 Intact — made `abstract_file_structure()` read
      1 GB past an 832-byte file. So "demand the witness" alone would not have
      fixed it: the missing check was the extent bound, which `validate()`
      already had and the read path never called. A flip in `IMAGE_BYTES`
      TITLE_SIZE was worse still — `validate_file_structure()` PASSED it,
      because nothing bounded that block's payload.

      What changed:

      * **A generated handle's `operator bool` is `validate()` passing** —
        both witnesses and every declared extent. The old bounds-only test is
        `in_bounds()`, called explicitly by code that reads damaged files on
        purpose: recovery, `generate_file_map`, `note_attributes`, and
        `validate_deep`'s out-of-bounds test. The read path's hops already
        read `if (!x) fail(x.validate())`, so they needed no change.
      * **Optional slots go through `present()`**: NULL_OFFSET is absent,
        anything else must validate or the read throws. The bool alone would
        have made a damaged ICC profile silently absent.
      * **`validate()` bounds `IMAGE_BYTES`' payload** — new
        `PAYLOAD_OVERRUN`, computed from `extent()`, the one formula.
        `store()`'s self-check window grew to match.
      * **Three pre-existing faults the new read-path sweep found**, all
        reachable on HEAD: the generated `fits()` computed
        `__offset + __header_size`, which wraps for a flipped NULL_OFFSET slot
        (0xFF..FE) and read before the mapping — now a subtraction, as
        `BlockHeader::fits` already was; `generate_file_map` looped on a
        damaged COUNT past EOF — now bounded by `ArrayHeader::readable()`,
        geometry not count; and orientation was cast from the decoded float
        into an enum of half BIT PATTERNS, so a clean 90-degree image read back
        as enum 90 rather than `ORIENTATION_90` (0x55A0) and a damaged negative
        angle was undefined behaviour — now the raw 16 bits.

      Measured: `ife_damage_sweep_tests` drives every public read entry point
      over all 14,475 single-site damages and every repaired copy — under
      ASan+UBSan, zero errors (10,712 read, 3,763 refused). A 64-bit random
      probe over both fixtures: zero errors; before the change it faulted on
      its first trial. Controls unmoved: recovery sweep accounting identical,
      bench output byte-identical, and `generate_file_map` plus the
      abstraction of every clean fixture identical.

      **The consequence to know about:** a structurally damaged file is now
      REFUSED by `abstract_file_structure()` instead of opened — 58–65% of
      one-bit trials throw. That is the honest answer, and `Recovery` +
      `apply()` is how to reopen one. Iris-Codec opens slides through this
      call without validating first (`IrisCodecSlide.cpp:216`), so it
      inherits the refusal — RC-10.8.

- (open: WBS.md) **RC-10.2 — A content-level metric.** No IFE test compares recovered
      *content* against a clean denominator; `ife_recovery_bench_tests` is
      reference-anchored and hashes a child's extent, which is close but not
      the leaves a reader actually returns. Extend it (or add a sibling) that
      runs `abstract_file_structure()` over the repaired file and diffs the
      attribute tree, image list and tile table against the clean run. This is
      the test that would have caught FastFHIR's 20,057, and it is the gate
      RC-10.1 needs to prove itself against.

      Starting point: RC-10.1's probe flattened the abstraction to leaf
      strings and diffed them against the clean run. It found 13 fabricated
      leaves over 40 one-bit trials on the 1.0 witness, and at most 6 in any
      single trial — against FastFHIR's 29,303 from ONE bit — but it did not
      classify them. The expectation is that every one is an unwitnessed
      inline scalar (the documented boundary); ASSERTING that is this item.
      One 2-bit trial produced 3, so "at most one leaf per flipped bit" is
      not a safe assertion.

- (open: WBS.md) **RC-10.3 — Iris-Slide must write tile frames. (Cross-repo; lockstep.)**
      `TILE_INDEX` is the only identity witness in the format, and RC-9.1 makes
      it decisive — but Iris-Slide's `src/IrisSlideEncoder.cpp` contains no
      `TilePixelDataCreateInfo` and no `TILE_INDEX` anywhere, so no real slide
      carries one. The corpus writer does
      (`tests/ife_corpus_writer_11.cpp`), which is why the 1.1 fixture proves
      the mechanism while the 1.0 fixture shows what its absence costs. The IFE
      half is done; this is the encoder half. Frames are optional by design, so
      this is a writer policy decision, not a format change.

- (open: WBS.md) **RC-10.4 — Block edges have no identity witness. (P2, design.)** A
      flipped offset landing on a valid block of the SAME type satisfies both
      witnesses at the new address, so `classify()` reports `Intact` and is
      right to — nothing on the wire contradicts it. Measured on the 1.0
      witness: METADATA's `ATTRIBUTES` slot moving between its three sibling
      `ATTRIBUTES` blocks. Closing this needs a third witness (a back-pointer,
      or an index field of the kind `TILE_INDEX` already is for tiles), which
      is a spec change and therefore append-only work. Until then the bench
      test counts these as `blind` and does not assert on them.

- [x] **RC-10.5 — Warnings on, both build systems.** `-Wall -Wextra -Wswitch`
      (`/W4` on MSVC), added once per build system: `add_compile_options` at
      directory scope in `CMakeLists.txt`, placed AFTER
      `FetchContent_MakeAvailable(IrisHeaders)` so the Iris-Headers subproject
      keeps its own flags, and appended to `_COPTS` in `BUILD.bazel`, which
      every first-party target — the test suite included, via
      `ife_tests(copts = ...)` — already threads. Deliberately NOT `-Werror`:
      a compiler upgrade must not be able to break the build, which is how a
      project ends up deleting the flags.

      Eight first-party warnings surfaced and were fixed, none by suppression:

      * `src/IFE_Recovery.cpp` — `audit_ref`'s `__base` and
        `enumerate_file_header`'s `root` were unused **parameters**, so the
        parameters are gone rather than voided. Each removal states its reason:
        a `BlockRef` already carries both witnesses (re-reading the bytes would
        let the audit and the enumeration disagree), and the root is at offset
        0 by definition (the caller still builds the handle as its validity
        gate, then has nothing to pass).
      * `generated_source/IFE_Blocks.hpp` ×5 — `AttributeSizeEntry::nested`
        had no default member initializer, so the ordinary `{key, value}`
        construction was a `-Wmissing-field-initializers` site. Fixed in
        `generator/emit/cpp.py`, never in the generated file; `nested` now
        defaults like `KIND` already did.
      * `tests/ife_recovery_tests.cpp` — `collect_victims`'s unused buffer
        parameter, likewise removed.

      **`-Wswitch` is a guard, not new coverage** — it is on by default in
      clang and gcc. Naming it says a new `RepairClass`/`GapClass` member must
      fail at compile time, and survives someone trimming `-Wall`. It only
      bites where the code gives it something to bite, and the `GapClass`
      tally in `recover()` was an if/else-if chain, invisible to it — exactly
      the shape of the FastFHIR bug where a gap class was assigned and counted
      by nobody. It is a `switch` now.

      Red-green verified by probe, not assumed: appending an enumerator to
      `RepairClass` warns at both switches (the tally and `plan_repair`), and
      to `GapClass` at the new tally switch. Clean rebuild of both build
      systems is warning-free; ctest 19/19, `bazel test //...` 17/17, sweep
      accounting unmoved. Only clang was available locally — the gcc and MSVC
      legs are on CI.

- [x] **RC-10.6 — `HoleCandidate::word` deleted.** It stored `read(x)` — the
      corrupted copy of an offset — and nothing read it. The word's only job is
      to price how likely position `x` is to be a real block whose VALIDATION
      was damaged, and that answer is `self_cost`, computed at admission; the
      word itself is never a matching target, because the parent's damaged
      offset is scored against `x`, which is exact. A field holding exactly the
      value the engine must never compare against is how someone compares
      against it later, so the field is gone and a comment in its place says
      why. Verified behaviour-neutral by A/B: the sweep reports the same
      14,475 damages / 1,573 holes / 216 hole repairs / 20 ambiguous / 0
      refused with the field present and absent.

- (open: WBS.md) **RC-10.7 — Recovery's entry walkers bound the slot's START, not its
      width.** `enumerate_images`, `enumerate_annotations`, the attribute
      walker and the tile-offsets walker cap entries at
      `(size - begin) / stride`. That keeps each entry's first byte in the
      file, but a STRIDE narrower than the entry — reachable only under
      multi-bit damage; the sweep's three single-site masks cannot narrow any
      of these strides — reads up to `entry_size - stride` bytes past EOF.
      Found by inspection while writing `ArrayHeader::readable()`, which is
      the fix; not yet reproduced. Move the walkers onto it, and hold the
      bench hash and sweep accounting identical while doing so.

- (open: WBS.md) **RC-10.8 — Iris-Slide now refuses damaged slides at open. (Cross-repo.)**
      `__INTERNAL__Slide`'s constructor calls `abstract_file_structure()`
      directly (`IrisSlide.cpp:216`, reached from `IrisSlideCache.cpp:55`
      with no `validate_file_structure` on that path). Since RC-10.1 that call
      throws on a structurally damaged file instead of returning fabricated
      values or faulting. Decide what a viewer does next: surface the
      diagnostic, or offer `Recovery` + `apply()` on a copy and reopen.

---

## Not in scope (yet)

IFE has no compact layout, so one FastFHIR piece does not port: REC-18.7's
refusal of compact streams (nothing to refuse). Corruption *fuzzing* (TSan)
and a corpus-scale damage harness stay on the release-order "Not blocking"
list. The FastFHIR-benchmark side of FastFHIR's same change set (F2/F3:
corrupt_stream parent-slot tuples, edge-level fingerprints) has no IFE analog
— there is no companion benchmark repository for IFE yet.

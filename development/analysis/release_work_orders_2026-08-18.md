# Release work orders R-1 to R-6

> **Record.** Written 2026-08-18 to 2026-08-19 in MIGRATION.md at the repository root; moved here on
> 2026-11-24 when `development/` replaced the root planning documents (git history
> keeps the originals; the last commit that holds them is 005347e). Kept as written,
> with local paths, sibling-checkout paths and a quotation restated for a public
> repository. Open items are tracked in [WBS.md](../WBS.md) under the same IDs; their
> boxes here read `(open: WBS.md)`. Dated records keep the names they had (Iris-Codec
> is now Iris-Slide).


Written 2026-08-18, after the Phase 6 audit corrected this document (three
items were done and marked open; the "Phase 6 is DONE" label belonged to the
generated-layer milestone inside it). These are what remain between here and
a released 1.1.

**Decided 2026-08-18** — do not re-litigate:

* The release is gated on the corpus **harness**, not on full 18-block
  coverage. Coverage keeps growing after the tag, with the gaps recorded.
* Bindings follow the tag: Python first, then JS/WASM.
* The 1.1 revision errata (the `ATTRIBUTE_SIZE` 6→7 correction) **stays out**
  of the published document. `tests/wire/README.md` is its record.

**Rules: the same ones as the cross-pollination orders below** — one task ID
per session, run *Locate* first and STOP on a mismatch, do not commit, never
hand-edit `generated_source/` or `generated_docs/`, append-only is inviolable,
and ⚠ marks a decision a flash model must not take alone.

## Priority summary

| ID | Priority | Task | Why | Status |
|---|---|---|---|---|
| R-1 | **P0** | Build the corpus harness | Four tests eat the corpus; nothing checks it still contains what it claims | **done (2026-08-18)** |
| R-2 | P1 | Close the reachable corpus gaps | All five missing blocks are writable today — the blocker was v1, now deleted | **done (2026-08-19) — 18 of 18** |
| R-3 | P1 | Release 1.1 | No tag has ever been cut on this repository | open — R-3.4 done (2026-08-19); R-3.1 held, the document had a normative gap |
| R-4 | P2 | Python bindings (pybind11) | Already specified in Phase 6; unblocked by the tag | open |
| R-5 | P3 | JS/WASM binding surface | Transport exists; only the binding is missing | open |
| R-6 | P2 | Expectations header for the 1.1 witness | 1.1's fields are pinned by digest and read by no test; the 1.0 pair is the pattern | open |

---

## R-1 — Build the corpus harness (P0) — ✅ DONE

**Why.** `tests/corpus/manifest.json` declares, per fixture, which block types
it covers. Nothing reads that claim. Four tests consume the fetched bytes as a
fixture, so a fixture that quietly stopped containing annotations would keep
every one of them green — which is exactly how `ANNOTATION_JPEG` was lost
before. §Conformance corpus → Harness calls its step 4 the step that keeps
this honest, and step 4 is the part that does not exist.

**Read first**
- §Conformance corpus → Harness — the five steps, which are the spec for this.
- `tests/ife_runtime_tests.cpp` — the walk it already does over one fixture.
- `tests/tests.cmake` — the corpus fetch, and how corpus-consuming targets are
  declared.

### Locate

```bash
# from the repository root
ls tests/ife_corpus_tests.cpp 2>/dev/null || echo "no harness yet (expected)"
grep -n "option(IFE_CORPUS" CMakeLists.txt tests/tests.cmake || echo "no opt-out yet (expected)"
python3 -c "import json;print(list(json.load(open('tests/corpus/manifest.json'))))"
```

**Expect:** no harness, no `IFE_CORPUS` option, and two fixtures listed.

### R-1.1 — Read the manifest from C++

The harness must iterate the manifest, and this repository has no JSON library
and wants none. Follow the precedent already in the tree: generate it.
`tests/fixtures/build_future_spec.py` is generated at configure and gitignored;
do the same here with a small script emitting a header of fixture names, sizes,
digests and declared block types.

**Done when:** a configure-time header lists both fixtures and their `blocks`
arrays, and is gitignored rather than committed.

### R-1.2 — The walk

`tests/ife_corpus_tests.cpp`, one target, per fixture: map it with
`IFE::Window::resident`; `validate_file_structure` must succeed; walk every
`points_to` edge, confirming each block's recovery tag and that its
`VALIDATION` word equals its own offset; record which of the 18 block types
were reached.

**Done when:** it passes over both fixtures and prints the block types observed.

### R-1.3 — The honesty gate

Compare observed against declared. A block the manifest **claims** and the walk
does not reach is an **error** naming the fixture and the block. A block
reached but not declared is also an error — the manifest is the record, so it
must be updated rather than drift.

**Done when:** deleting one entry from a fixture's `blocks` array turns the
test red, and adding a bogus one turns it red the other way. Red-green both.

### R-1.4 — Wire it

CMake target + ctest, a Bazel `cc_test`, and the `-DIFE_CORPUS=OFF` opt-out the
plan promises and nobody built. **The opt-out disables only this test target,
never the fetch** — the oracle and the >4 GiB test depend on the snapshot.

**Done when:** `ctest` is 15/15, `bazel test //...` is 13/13, and
`cmake -DIFE_CORPUS=OFF` drops exactly one test and still configures.

> **Status (2026-08-18): R-1 is DONE — R-1.1 through R-1.4.**
> `tools/corpus_manifest_header.py` projects the committed manifest into a
> gitignored header at configure (CMake) and by genrule (Bazel);
> `tests/ife_corpus_tests.cpp` validates each slide fixture and walks it with
> `generate_file_map` — the traversal that already exists, rather than a
> second one to be wrong — then compares reached against declared in both
> directions. `manifest_name()` is a switch with no `default`, so a new
> `MapEntryType` is a compile error here rather than a silent pass.
> `-DIFE_CORPUS=OFF` drops the test target only; the fetch is untouched and
> the other three corpus tests still get their bytes. ctest 15/15, bazel
> 13/13, `--validate`/`--check`/portability lint clean.
>
> **Two later corrections to what is described here** — see R-2's closeout: the
> harness now unions `recover_file_structure` into the walk (a tile frame is
> invisible to the offset graph), and it counts *observed* blocks rather than
> declared ones (it briefly over-reported coverage for fixtures that failed to
> load). What it still does not do is exercise each block's functions: that is
> R-6.
>
> **It caught a real drift on its first run.** The snapshot fixture contains
> `TILE_PIXEL_DATA` — `tests/ife_snapshot_writer.cpp` writes 16 B of pixel
> data per tile across 84 tiles, and the tile-offsets entries address it —
> and the manifest never declared it. The corpus therefore reaches **14 of
> 18** block types, not the 13 recorded below, and the remaining gap is four
> blocks rather than five. Manifest corrected in the same change. Red-greens
> both ways: declaring a block the fixture lacks (`CLINICAL_METADATA`) errors
> with "manifest declares … the walk never reached it"; undeclaring a real
> one (`ICC_PROFILE`) errors with "the walk reached … which the manifest does
> not declare".
>
> One manifest schema addition was needed: `kind` (`"slide"` or
> `"fragment"`, defaulting to slide). `v1_tile_offsets_full_width.bin` is a
> bare 32-byte `TILE_OFFSETS` block, not a file — it has no file header to
> walk from, so it is size-checked and **excluded from coverage accounting**
> rather than silently credited. The plan above assumed every fixture was a
> whole slide; it is not.

---

## R-2 — Close the reachable corpus gaps (P1) — ✅ DONE

> **Restructured 2026-08-18 (decision) — the corpus is a set of frozen
> per-version witnesses, each covering everything its version defines.**
> Not one moving file: the newest witness *is* the feature corpus while its
> version is draft, and freezes when the version ratifies. The set grows; no
> entry is ever rewritten, so the version trail is immutable and old-file /
> new-reader stays provable against real bytes — which a corpus regenerated at
> head would delete on its first regeneration (`ife_v1_oracle_tests` asserts
> `declared == 0x00010000u` and reads every field through a handle built at the
> version the file declares).
>
> **No split by tile encoding.** Measured across the whole field schema, the
> only thing that varies with encoding is the `ENCODING` byte itself; nothing
> else keys off it, and `CIPHER_OFFSET` is NULL for both JPEG and AVIF. A
> `corpus_AVIF` would be structurally identical to `corpus_JPEG` but for one
> enum byte, in a repository whose scope excludes compression. Real AVIF bytes
> are Iris-Codec's (now Iris-Slide's) evidence, not this layer's.
>
> **CIPHER (was R-2.2 ⚠) — resolved by isolation.** The encoding rule is
> normative prose only; nothing in the validator enforces it. And the block is
> content-free by design — "encoders write nothing beyond the universal header
> and readers ignore its contents" — so a CIPHER fixture invents no payload.
> What it cannot do is sit in a conformant JPEG witness, because
> `CIPHER_OFFSET` shall be NULL_OFFSET unless `ENCODING` is
> `TILE_ENCODING_IRIS`. So it gets its own file claiming IRIS, which confines
> the unreal claim to one small fixture and leaves the witnesses realistic.
>
> **Built and verified (2026-08-18); hosted and green 2026-08-19:**
>
> | File | Bytes | sha256 | Covers |
> |---|---|---|---|
> | `v1_1_witness.test_slide` | 859 | `1293df66…` | 17 of 18 + `TILE_FRAME` |
> | `v1_0_witness.test_slide` | 3151 | `aab8dec8…` | 16 — every 1.0 block but CIPHER |
> | `cipher_iris.test_slide` | 869 | `90a36c79…` | all 18 + `TILE_FRAME` |
>
> Both validate; both byte-reproducible across runs. The earlier 574-byte
> draft is superseded — do not host it.
>
> **A writer bug this caught, worth keeping.** `store()` for a tile frame takes
> the **anchor** (the first byte of the stream), not the frame's own start:
> `TILE_PIXEL_DATA::offset::VALIDATION` is **-5**, a displacement *behind* the
> anchor, and the runtime builds the handle the same way. Passing the frame
> start writes it five bytes early — which the recovery scan still accepts,
> because a u40 storing its own position is self-consistent wherever it lands,
> so the frame appeared "covered" while attached to no stream at all. It
> surfaced only because the misplaced write landed inside the CIPHER block and
> broke validation there. A presence-checking harness would never have caught
> it; a function-checking one would.
>
> **Update (2026-08-18, later): the restructure is complete in the tree and
> the corpus reaches 18 of 18 blocks.** Names settled as
> `v1_0_witness` / `v1_1_witness` / `cipher_iris`, renamed across every test
> source, `tests.bzl`, `tests/corpus/BUILD.bazel` and the docs; the manifest
> gained `role` and now lists four fixtures.
>
> | Fixture | Bytes | sha256 | Blocks |
> |---|---|---|---|
> | `v1_0_witness.test_slide` | 3151 | `aab8dec8…` | 16 — every 1.0 block but CIPHER |
> | `v1_1_witness.test_slide` | 859 | `1293df66…` | 17 + `TILE_FRAME` |
> | `cipher_iris.test_slide` | 869 | `90a36c79…` | 18 + `TILE_FRAME` |
>
> The 1.0 witness gained the two annotation-group blocks (both 1.0 fields), so
> `tests/ife_v1_fixture.hpp` gained `annotation_groups` — two groups of
> different title lengths and member counts, because one group cannot prove the
> byte run is sliced and equal-sized groups cannot prove the slice moves with
> the entry. The oracle now *verifies* them rather than asserting their
> absence: title and 24-bit members decoded out of the shared run at the
> boundary each sizes entry declares, ending exactly at the run's end. Two
> block counts in `ife_runtime_tests` moved 12→14 and 11→13.
>
> **A latent build defect surfaced doing it:** `ife_snapshot_writer` never
> linked the baseline `IFE_Blocks.cpp`, so the target did not link at all. It
> is `EXCLUDE_FROM_ALL` and nothing depends on it, so nobody had noticed.
>
> **ctest is 15/15; `bazel test //...` is red with a 404 until the three
> objects are uploaded** — its genrule fetches into a fresh `RULEDIR` every
> time and can never use the local cache, which is the same property that makes
> it the honest check. CMake passes here only because `.deps/corpus` holds the
> digest-matching objects.
>
> **Closed out 2026-08-19.** All three objects are hosted; `ctest` is 15/15 and
> `bazel test //...` is 13/13 against them. Two defects surfaced during the
> upload round-trip and are fixed:
>
> * The `ife_corpus_tests` Bazel target declared only two of the four fixtures
>   as `data`, so two were absent from the runfiles tree. CMake had not caught
>   it — it passes the corpus *directory*, so a new fixture needs no build
>   change there, while Bazel builds runfiles from an explicit list. Recorded
>   in `tests/corpus/README.md` as the third step of adding a fixture.
> * **The harness counted declared blocks rather than observed ones**, so it
>   printed "18 of 18" in the same run where two fixtures failed to open. The
>   count now comes from what the walk reached; verified against a deliberately
>   incomplete corpus, where it reports 16 of 18 and fails. A coverage gate that
>   over-reports on failure is the exact defect the gate exists to prevent, and
>   it was inside the gate.
>
> Also lost and restored in the same round-trip: `v1_tile_offsets_full_width.bin`
> 404'd after the upload (the prefix appears to have been replaced rather than
> added to). It is 32 bytes and the only fixture whose `u40`/`u24` fields have
> every byte significant — in a normal small slide those high bytes are always
> zero, so a truncating load or store reads back correctly and hides.
>
> **Optional values filled (2026-08-19).** Coverage of optional *blocks* was
> already complete — all eight nullable offsets are reached — but every 1.1
> optional *value* was sitting at its default, which for several of them is
> indistinguishable from absence. The 1.1 witness now carries `TILE_LENGTH` 128
> (zero and absence both mean 256, so an unset field cannot tell a decoder
> honouring it from one assuming the default), layer `Z_PLANES` 3, per-stream
> frame `Z_PLANES` varying 3/0/0, and one `NULL_TILE` slot so a sparse layer
> exercises the sentinel. Verified by reading each back through the handles:
> the 1.1 witness returns the set values and the **1.0 witness returns absent
> for every one**, which is the version gate proven on real bytes rather than
> only on the synthetic 200.0 fixture. Fixtures re-pinned — `v1_1_witness` 832 B
> `e93a2018…`, `cipher_iris` 842 B `7ddfa5aa…` — and re-uploaded.
>
> Still uncovered by design: tile encodings other than JPEG, and
> `clinical_encodings` values beyond `CLINICAL_HL7_V2`.
>
> **Still open:** R-1's harness checks block *presence*, not every function of
> every block — it does not read any of the values above, so nothing would
> notice if they regressed. The frame-anchor bug and this are the same gap:
> R-6.

> **Superseded:** the 1.0 witness is not yet comprehensive (16 of the 17 blocks
> 1.0 defines — it lacks the annotation groups), and making it so touches
> `tests/ife_v1_fixture.hpp`, which the oracle reads its expectations from.
> The manifest needs `role`/`covers_version` and the renames. And R-1's harness
> still checks block *presence*, not every function of every block.


**The blocker recorded below is stale.** §Conformance corpus says the
annotation-group blocks are "still blocked: no group writer". That was v1's
writer, deleted in Phase 6. Verified 2026-08-18: `CipherCreateInfo`,
`AnnotationGroupSizesCreateInfo`, `AnnotationGroupBytesCreateInfo`,
`TilePixelDataCreateInfo` and `ClinicalMetadataCreateInfo` all exist in
`generated_source/IFE_Blocks.hpp` and are used by `IFE_Blocks.cpp`. Every one
of the five missing blocks is writable today. What is missing is a fixture
generator, not a capability.

### R-2.1 — A second fixture

Extend `tests/ife_snapshot_writer.cpp` to emit a fixture carrying the four
straightforward gaps: both annotation-group blocks, a framed
`TILE_PIXEL_DATA`, and `CLINICAL_METADATA` (1.1, one per
`clinical_encodings` value worth covering). Host it, add it to the manifest
with size, digest and its `blocks` array.

**Done when:** the R-1.3 gate observes all four in the new fixture, and the
corpus reaches 17 of 18 block types — from 14, not the 13 the section below
recorded before R-1's harness corrected it.

> **Status (2026-08-18): R-2.1 is built and verified; it needs an upload.**
> `tests/ife_corpus_writer_11.cpp` emits a 574-byte 1.1 fixture, sha256
> `92869ecd6fb47a0b2e16071fb2a17e80492f0af4e10286c2dcf03113d8c423b3`, carrying
> one named annotation group, an HL7 v2 clinical stream, and framed tile
> streams. Verified against the real gate by placing it in `.deps/corpus/`
> with its manifest entry staged — `fetch_corpus.py` takes a digest-matching
> cached file without touching the network, so the whole path runs locally:
> validation passes and **the corpus reaches 17 of 18 blocks plus
> `TILE_FRAME`, from 14**. Two independent compilations produced byte-identical
> output.
>
> **The manifest entry is deliberately NOT staged in the tree.** The file has
> to be uploaded to `r2://ife-validation/spec-validation/` first; an entry
> naming a URL that 404s fails `fetch_corpus.py`, which is a
> `message(FATAL_ERROR)` at configure — it would break every build, not just
> the corpus test. Entry is ready to paste once the object is live.
>
> **Two corrections to the task as written.**
>
> 1. *"Extend `tests/ife_snapshot_writer.cpp`"* is wrong. That writer is built
>    against the **1.0 baseline** schema (`build_baseline_spec.py`) on purpose,
>    so the snapshot is 1.0 bytes a 1.1 reader must cope with, and its content
>    is pinned field-for-field to `tests/ife_v1_fixture.hpp` because the oracle
>    reads its expectations from there. 1.1 content cannot come out of a 1.0
>    schema, and adding it would break the oracle. Hence a second writer, built
>    against the current layer.
> 2. *The frame is invisible to the harness R-1 built.* `generate_file_map`
>    walks the offset graph; a tile frame carries **no recovery tag** and is
>    addressed backward from the stream it precedes, so only
>    `recover_file_structure`'s signature match finds one. R-1's harness now
>    unions both maps. That also means the corpus exercises the recovery path
>    over real bytes, which nothing else did — on a healthy file recovery finds
>    a subset of the graph (measured: 12 of 14 on the snapshot, missing
>    `FILE_HEADER` and `TILE_PIXEL_DATA`, both untagged).
>
> One more false green caught in passing: the harness first reported "18 of
> 18" with `CIPHER` still unreached, because it counted `TILE_FRAME` — a
> prefix, not one of the 18 blocks — in a numerator whose denominator excluded
> it. Now counted separately.

### R-2.2 ⚠ — CIPHER needs a decision

`CIPHER_OFFSET` **shall** be `NULL_OFFSET` unless the tile encoding is
`TILE_ENCODING_IRIS`, and that encoding is "reserved, unused". A `CIPHER`
fixture therefore has to claim an encoding no encoder produces — a
conformant file that no real pipeline could have written. Either that is
acceptable evidence, or `CIPHER` is recorded as permanently uncovered by real
bytes and covered by the generated round-trip alone.

**Produce the analysis and STOP.** This is a judgement about what the corpus
is for, not a coding task.

---

## R-3 — Release 1.1 (P1)

No tag has ever been cut on this repository, and no GitHub release exists on
either remote. Everything below is release mechanics; none of it is blocked
by R-2.

### R-3.1 — Ratify the revision

`spec/ife_header.json` carries 1.1 as `"status": "draft"` with no date. Set it
ratified with an ISO date — `generator/validate.py` requires a date on a
ratified revision, so the gate enforces this. The cover page and the revision
history are generated from that entry.

**Done when:** `python3 -m generator --validate` exits 0 and the rendered
document's cover no longer says draft.

### R-3.2 — Refresh the wire witness

If the spec moved at all, `tools/refresh_witness.py` records the baseline in
the same change. `--check` fails until it does.

### R-3.3 — Publish the document

`spec/build_document.sh` produces the PDF and HTML. Its three gates are the
release gate for the document; the exit code is not enough, because
Asciidoctor exits 0 on a missing include.

### R-3.4 ⚠ — Release artifacts and a README claim

The README states pre-compiled binaries are provided "for all major systems
under the **releases tab**". No release exists, so that sentence is currently
false. Either the release workflow builds those binaries or the README is
corrected. **Decide before tagging** — shipping a first release that
contradicts its own README is worse than shipping without binaries.

**Exit:** a tag exists, the specification is published from the pipeline, and
every claim the README makes about artifacts is true.

> **Status (2026-08-19): R-3.4 is done; R-3.1 is deliberately held.**
>
> **A blocker this task did not record: the 1.1 document is normatively
> incomplete.** `spec/ife_spec.adoc` carried three `TODO(draft)` comments —
> AsciiDoc line comments, so they render as nothing and the published PDF
> simply lacked the content. One was load-bearing: the document required "the
> global tile index" in five clauses (`TILE_INDEX *shall* encode the global
> tile index`; `The count *shall* equal the total number of global tile
> indices derived from the layer extents array`) and defined it nowhere, while
> Terms cross-referenced `<<ife-tile-offsets>>` for the axis — the very
> section the TODO sat in front of. An implementer could not have written a
> conformant encoder from the published document.
>
> **Closed.** `[[ife-tile-indexing]] ==== Global Tile Indexing` now states the
> scheme from the reference figure: indices ascend by layer and row-major
> within a layer from the upper-left tile, _base_(_l_) is the tile count of
> every preceding layer, and _G_(_l_, _x_, _y_) = _base_(_l_) + _y_ ×
> X_TILES~_l_~ + _x_, with the inverse and the total stated as clauses. The
> derivation is checked against real bytes rather than asserted: the snapshot
> writer's extents are 2 × 2, 4 × 4 and 8 × 8 at ascending scale — 84 tiles,
> the figure's exact geometry — so the worked table in the section (0–3, 4–19,
> 20–83) is the corpus fixture's own indexing. Four cross-references that had
> no target now resolve to it, including the Terms one, which also said
> "global *slide* indexing scheme" for the term the document defines as
> "global *tile* indexing scheme".
>
> **Still deferred, and not blocking:** the other two `TODO(draft)` markers —
> Equations 2.1/2.2 (MpP and Mc normalization) and Figure 2.7 (slide space).
> Both are a figure and its equations for rules the narrative already states
> in prose beside the field and in Terms respectively, so the document is
> complete without them and poorer for it. They want the v1.0 document, which
> exists in no checkout here.
>
> **R-3.1 held by decision (2026-08-19):** the revision stays `draft`
> until the migration above is reviewed. `spec/ife_header.json` is untouched,
> so R-3.2 has nothing to refresh — the narrative is not part of the wire
> witness, and `--validate`/`--check` are green without it.
>
> **R-3.4 is done, resolved the way the ⚠ asked: the workflow builds the
> binaries.** `.github/workflows/release.yml` — `verify` → `package` →
> `document` → `publish`, tag-triggered, with `workflow_dispatch` running
> everything except publish so the pipeline can be exercised before the first
> tag. Four archives: linux-x86_64, linux-arm64, macos-universal (one archive,
> both architectures) and windows-x64, each an install tree plus LICENSE,
> published with the specification PDF/HTML and a `SHA256SUMS`.
>
> Two pieces carry the reasoning rather than the YAML:
>
> * `tools/release_guard.py` — the tag, the declared version and the
>   ratification status have to agree before a compiler runs, because the fix
>   for a disagreement is a retag. It **refuses to release a draft**, so it
>   fails today, by design and on purpose. Red-greened over seven cases
>   (draft, ratified, `v1.1`, `v1.1.3`, wrong version, malformed tag, output
>   emission). It also renders the release body from the revision summary, so
>   the changelog is not typed a second time.
> * `tools/package_smoke/` — a consumer of the *installed* package:
>   `find_package(IrisFileExtension CONFIG)`, the umbrella header, and a call
>   into the shared library. Nothing else in the tree ever links the shipped
>   artifact — the tests and examples link the object library — so this is the
>   only check that an archive is more than a correctly named tarball, and on
>   MSVC the only place the DLL's import library is linked at all. It found a
>   real one immediately: `BYTE` is `Iris::BYTE` to a consumer, unqualified
>   only inside tests that bring their own using-directive.
>
> Verified locally on macOS end to end: configure → build → `cmake --install`
> (10 files: 6 headers, 2 libraries, 2 package-config files) → smoke consumer
> builds, links `@rpath/libIrisFileExtension.dylib`, and runs. The document
> builds through all three of `build_document.sh`'s gates, HTML and PDF.
>
> **Unverified, and where the risk sits:** every platform but macOS-arm64. The
> other three matrix legs, the Windows loader paths, `ubuntu-22.04-arm` as a
> runner label, and the whole publish job have never executed. Run the
> workflow by dispatch once before trusting it — that is what the dispatch
> trigger is for. A Windows consumer also gets no `IFE_IMPORT_API`, so
> `IFE_EXPORT` is empty rather than `dllimport` on that side of the install;
> functions link through the import library regardless, but that is the corner
> the smoke test is most likely to fail in first, and changing it is an
> export-contract task, not a packaging one.

---

## R-4 — Python bindings (pybind11) (P2)

Fully specified already in [§Phase 6](migration_phases_1_to_6.md#phase-6--ecosystem-cutover--release) —
bind the `Abstraction::` surface, ship behind its own CMake option, generate
enum values and field keys from `spec/*.json` rather than restating them, and
add a CI job, because a binding that is not built is a binding that is broken.
Nothing about that analysis changed; it was waiting on the tag.

---

## R-5 — JS/WASM binding surface (P3)

As R-4. The residency work is done — `IFE::Window`'s Emscripten branch fetches
byte ranges on demand, and the `wasm` CI job compiles it — so what is missing
is the binding, not the transport.

---

## R-6 — Give the 1.1 witness an expectations header (P2)

**Scope, first, because the obvious reading is wrong.** This is *not* "read
everything". Tile pixel streams are out of scope permanently: IFE references
them in their on-disk compressed form and never interprets a byte, because
compression is Iris-Codec's (now Iris-Slide's) layer. The witness writer fills them with `0xCD`
and nothing should ever look.

What *is* in scope is the structural field values, and the precedent is already
here: `ife_v1_oracle_tests` makes 106 assertions against
`tests/ife_v1_fixture.hpp` — `encoding()`, `x_extent()`, per-layer
`x_tiles()`/`scale()`, `microns_pixel()`, every annotation entry field, the ICC
and attribute byte runs. Those accessors are generated in this repository; a
wrong derived offset is this repository's defect to catch.

**The gap.** That treatment stops at the 1.0 witness. The 1.1 witness has no
expectations header, so everything 1.1 added is pinned by digest and read by
nobody — including the optional values just written: `TILE_LENGTH` 128, layer
`Z_PLANES` 3, per-stream frame `Z_PLANES` 3/0/0, one `NULL_TILE` slot,
`MICRONS_PLANE`. A digest pin catches a change; it does not catch a *reader*
that stops returning them correctly.

**Shape.** A `tests/ife_v11_fixture.hpp` beside the 1.1 writer, declaring what
that fixture contains, and a test asserting it — mirroring the 1.0 pair rather
than inventing a second idiom. The assertions that matter most are the
version-gated ones: every 1.1 field reads its value on the 1.1 witness and
**absent** on the 1.0 witness. That pair is the bidirectional guarantee stated
as a test instead of as prose.

**The frame anchor is already covered (2026-08-19)** and is no longer part of
this task. `ife_validation_tests.cpp` pins both failure modes on synthetic
buffers, needing no fixture: storing the tile-offsets entry's own value in the
VALIDATION slot (which `validate()` rejects), and storing a correct frame at
the wrong place (which `validate()` cannot reject, because it is internally
consistent — what catches it is validating at the offset the entry names).
Both red-greened.

**Done when:** every 1.1 field is asserted present-and-correct on the 1.1
witness and absent on the 1.0 witness, and re-introducing the frame-anchor bug
turns a test red.

---

## Not blocking the release

Carried forward so they are not lost, none of them gating: corruption tests;
diagnostics generated from the JSON's normative clauses; TSan and fuzzing; a
fixture with real AVIF tile bytes. The Windows arena branch and the
`METADATA_FREE_TEXT` question, both previously listed here, are closed — see
[§Phase 6](migration_phases_1_to_6.md#phase-6--ecosystem-cutover--release).

---

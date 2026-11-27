# Contributing to Iris-File-Extension

Thanks for contributing. IFE is a binary slide format — the bar for
correctness is high, and a few rules are absolute. Read `README.md`,
`development/README.md` (how a change moves through review),
`development/ARCHITECTURE.md` (the decisions the code follows) and this page
before opening a PR.

## The absolutes

1. **Append-only is the wire contract.** Never move, resize, retype or remove
   a field that already exists in `spec/*.json`. New facts arrive only by
   *addition* — a new version group, a new field at the end of a group, a new
   block with a new recovery tag — and retirement happens only via
   `deprecated`. `python3 generate_IFE.py --validate` enforces this against the
   committed witness; a PR that breaks it will be rejected regardless of its
   other merits.
2. **Never hand-edit or commit generated output.** `generated_source/` and
   `generated_docs/` are build products, written by `generate_IFE.py` into the
   build directory at configure time, with a DO-NOT-EDIT banner. Fix the
   emitter in `generator/emit/` and regenerate.
3. **`tests/wire/witness.json` is evidence, not output.** It is committed on
   purpose — the gate compares against it — and is refreshed only as part of a
   reviewed wire change (see "What a wire change requires" below). A witness
   update without a corresponding spec change is a red flag. Its history is
   not unbroken: it records two deliberate breaks of the invariant, each a
   code-owner decision, the 1.0 `ATTRIBUTE_SIZE` correction and the
   reassignment of tile encoding value 3 to `TILE_ENCODING_AV1` (D26), both in
   `tests/wire/README.md`.
4. **The corpus is pinned, not trusted.** `tests/corpus/manifest.json` pins
   each hosted fixture by SHA-256; a re-uploaded file the manifest has not
   blessed is rejected, not silently adopted. Never touch corpus bytes without
   updating the manifest in the same change.

## The two guards: the contract and the bytes

IFE's compatibility is defended twice, at two different times, and the two
guards are not interchangeable:

| | `tests/wire/witness.json` — the contract | `tests/corpus/*.test_slide` — the bytes |
|---|---|---|
| What it is | Every fact the spec puts on the wire — recovery tags, per-version field (name, type, width, offset), entry sizes, enum values, named constants — derived from `spec/*.json` through `generator.model.layout.derive_layout`, the same derivation the emitters use | Real files on disk, pinned by SHA-256, hosted on Cloudflare R2 |
| When it runs | `python3 generate_IFE.py --validate` — before generation, before any C++ compiles, in CI | `ctest` — after a full build |
| Coverage | **All 18 blocks, every version group, every enum member, every constant** | **All 18 block types**, plus the optional tile frame, across one frozen witness per version (1.0 and 1.1) and a `CIPHER` variant — so 1.1 additions are covered by real bytes, not only by the contract |
| What it catches | Any shipped field moved, widened, retyped or removed; any enum or constant value changed | Anything that stops a real file decoding end-to-end — including derivation bugs the witness shares with the generator |
| What it permits | Additions, silently — that is what append-only means | — |
| Error it gives | Names the field and both offsets, e.g. `CHANGED blocks.TILE_TABLE.fields.1.0: 'X_EXTENT' ['u32', 4, 36] → ['u32', 4, 40]` | A failed test; the bisect is yours |
| Refresh | Deliberate, reviewed, committed — exact command in `tests/wire/README.md` | A manifest entry, an upload, and a `data` entry in `tests/tests.bzl` — see `tests/corpus/README.md` |

**Why both.** The witness is complete, precise, and instant, but it is
derived from the same schema and the same `derive_layout` the generator uses —
it can be wrong in exactly the way the code is wrong, and a shared bug passes
both. The pinned corpus bytes are the one check that cannot be fooled by a
reader and a writer agreeing with each other about the wrong thing. The
corpus, in turn, pins bytes rather than the schema, and it checks that a block
is *present and walkable*, not that every function of it behaves. **Contract first,
bytes second**: the witness catches the schema edit at `--validate` time, the
corpus catches what the implementation actually does at `ctest` time — and
neither replaces the other.

## What a wire change requires

1. Edit `spec/*.json` append-only — addition, never mutation.
2. `python3 generate_IFE.py --validate` stays green (the gate permits additions).
3. Refresh `tests/wire/witness.json` deliberately, in the same change. A
   baseline that does not record a new field cannot catch that field's later
   mutation — the refresh is what keeps the gate growing with the spec. CI
   enforces this: `--check` fails until the refresh lands (`--validate`
   prints a non-fatal note), so the discipline is mechanical, not just
   documented.
4. Corpus bytes change only when coverage deliberately grows, as a manifest
   entry plus an upload in the same change. A rule-1-compliant edit never
   breaks a pinned file, so it never *requires* a corpus change.
5. **A ratified version's witness is frozen.** Corpus fixtures are one witness
   per IFE version; the newest is mutable while its version is draft and fixed
   when it ratifies. Growing coverage adds a fixture, it does not rewrite a
   shipped one — a rewritten 1.0 witness would delete the only real-bytes proof
   that a current reader still reads 1.0 files.

## Build & test

```bash
python3 generate_IFE.py --validate     # spec consistency + append-only gate
python3 generate_IFE.py && python3 generate_IFE.py --check   # into build/generated_source/
cmake . -B build -DIFE_BUILD_TESTS=ON && cmake --build build --config Release -j \
  && ctest --test-dir build -C Release --output-on-failure
```

Traps that have already cost this project time:

- The generator's determinism is tested, not assumed:
  `tests/generator/test_determinism.py` runs it twice into temp trees and
  compares every emitted file (ctest `ife_generator_determinism`; CI adds
  `PYTHONHASHSEED=0/1`, where dict/set iteration order is where
  non-determinism would enter).

- A green `tests` job says nothing about whether the corpus host is up. The
  cache key is the manifest digest, so an unchanged manifest means a cache hit
  and no fetch at all. That is intended — nothing in that run needs the host —
  but do not read it as evidence the fixtures are reachable. To check that
  directly: `python3 tools/fetch_corpus.py --manifest tests/corpus/manifest.json
  --dest "$(mktemp -d)"`, which forces a real fetch and verifies every digest.

- A spec addition without a refreshed `tests/wire/witness.json` makes
  `--check` fail (`--validate` only notes it). Refresh deliberately with
  `python3 tools/refresh_witness.py`, in the same change as the spec edit.
- `-DIFE_BUILD_TESTS=ON` is required; without it ctest reports "No tests were
  found" and that reads as success.
- Chain build and test with `&&` — ctest reports PASS from stale binaries
  after a failed build.
- The generated layer and the test fixtures are generated at configure time,
  into the build directory. The spec, the witness and the generator are
  configure dependencies, so `cmake --build` after an edit reconfigures and
  regenerates first.
- The corpus is fetched at configure into `.deps/corpus/`, inside the
  `if(IFE_BUILD_TESTS)` guard, and a failed fetch is a `message(FATAL_ERROR)`
  (`tests/tests.cmake`). It breaks the whole configure, not just the corpus
  test — which is why a manifest entry must never name an object that is not
  yet hosted. A build with tests off never contacts the host at all.

## The code rules

What a pull request's code is held to in this repository, beyond the
absolutes above. The family's general conventions (control flow, resources,
prose) are kept privately; the review bot's findings name the rule they apply,
and where this list is more specific, it wins.

### The specification and generated code

The format is defined by `spec/*.json`, and the C++ that lays out its bytes is
generated from them (`spec/README.md`, `generator/README.md`).

- **The JSON is the one source of every wire fact.** An offset, a size, a
  recovery tag or an enumeration value is derived by the generator; code,
  tests and documents refer to the generated name instead of a copied number,
  which would drift silently from the specification.
- **Append-only, and generated output is never edited or committed** (the
  absolutes, above). A fix goes into the emitter under `generator/emit/`.
- **Little-endian on disk, on every host.** File bytes are read and written
  through `include/IFE_Bytes.hpp` or the generated handles; a pointer into the
  file is never cast to a multi-byte type, since that reads host byte order.
- **No reserved identifiers in new names**: no leading underscore followed by
  an uppercase letter, no two underscores in a row, no leading underscore at
  global scope. Existing parameters named `__name` are renamed only by a
  change that is about them.

### Errors at the public boundary

- **Every public function returns an `Iris::Result`** under
  `Iris::Domain::IRIS_FILE_EXTENSION`, carrying an error code, the offending
  value and where it failed, and is `noexcept`; no exception crosses it.
- **A value is written through the function's last, non-`const` reference
  parameter**, only on success, so a failed call leaves it as the caller had
  it: `Result abstract_file_structure(const FileAccessInfo&,
  Abstraction::File&) noexcept` (D30). There is no value-carrying result type
  (`ResultOf<T>`, `Expected<T>`, `std::expected`): one mechanism serves every
  boundary.
- **Inside the library the code throws** at the point of failure. Each public
  function's body runs inside `boundary()` (`src/IFE_Failure.hpp`), which
  catches by type: a thrown `Failure` keeps its `FileError` and the place it
  was thrown, and `catch (...)` is only its last handler. Nothing is printed
  to `std::cout` or `std::cerr` in place of returning the failure.

### Types and names

- **A `switch` over an enumeration is exhaustive, with no `default:`**, so a
  new member is a compile error at every switch that misses it (CLAUDE.md,
  Warnings).
- **The format's values are referred to by their generated names**, which
  come from `spec/*.json`; a name becomes its raw number only at the file.
- **Shared types take the plain name.** A type owned by `std::shared_ptr` is
  known by the pointer alias; the class it owns is that name with the suffix
  `_t`, in namespace `Iris`. An object made from a descriptor is made by
  `create_<name>(const <Name>Desc&, <Name>& out)` returning a `Result`, as
  `create_parser(const ParserDesc&, Parser&)` (adopted 2026-11-26, R-8); the
  descriptor is a separate `<Name>Desc` because a nested type cannot hang off
  a pointer alias.
- **A data member** is `_leading_underscore` or `m_name`, as the surrounding
  code does.
- **Iris-Headers' vocabulary is reused**: `Result`, `Buffer`, `Memory` and
  the rest. A new IFE type that does what one of them does is a finding.

### Comments

Block comments state the intent where the *why* is not visible in the code:
the write head's claim protocol, the tile frame's backward layout, byte order
and recovery tags. A comment that restates the code is replaced by clearer
code, and dead code is deleted; git keeps the history.

## Picking work

Each work item keeps the ID it has carried since it was filed (`RB-5`,
`RC-10.4`, `R-3.1`, `ReviewRubric_1`); the work breakdown that lists them is
kept privately. Name the branch after the item (`<kind>/<WorkPackage>_<n>`),
keep the diff scoped to that item, and name the items and the requirement
rows it advances in the pull request (`development/README.md`, update rule).

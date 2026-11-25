# Work Breakdown Structure

The tracking sheet for Iris-File-Extension's work. Each **work package**
states its purpose and its status; the **items** under it are the checkable
steps, red first, each ticked with its evidence and a link to the commit that
did the work. Requirements live in [RTM.md](RTM.md); decisions, the block
register and the decisions owed in [ARCHITECTURE.md](ARCHITECTURE.md); the
pipeline rules in [PIPELINE.md](PIPELINE.md); long write-ups and the full
record of closed work in [analysis/](analysis/).

The work-item IDs are the ones the retired root documents gave (`MIGRATION.md`,
`TASKS.md`, 2026-08 to 2026-11), kept so every reference in code, commits and
pull requests still resolves. IDs added on 2026-11-24 for items that had none:
ZC-1 to ZC-5, BL-1 to BL-7, RB-0.d and CI-3.6.

## Conventions

- **A work package is one branch and one pull request.** The branch is
  `<kind>/<Name>_<ID>` after the item it delivers (`feature/ReviewBot_CI-3`),
  chosen before the first push, since renaming a pull request's head branch
  closes the pull request. A pull request names the items it completes or
  advances; [README.md](README.md) gives its sections.
- **Red first.** An item that changes behaviour starts with a test shown
  failing, with the failure quoted; it is ticked only with the green evidence.
  An item whose product is a measurement or a document is ticked with the
  numbers or the review.
- **One tick, one piece of evidence, one commit link, on the tick's own line:**
  `- [x] CI-2.4 **Title.** evidence [1234567](https://github.com/Aptix-Bio/Iris-File-Extension/commit/<full hash>)`.
  The review bot checks every new tick in this file (`.github/review/rubric.toml`,
  `[trace]`).
- **VERIFY** is the maintainer's check; **Gate** is CI green on the pull request.
- **Status** after a package's name: `todo`, `doing`, `blocked` (with what it
  waits on), `done`. A blocked item does not hold its package: the package
  merges without it.
- **Closed work** is condensed in the table at the end; its full record,
  evidence included, is the `analysis/` file the row names.

## Plan

```mermaid
flowchart LR
    CI3["CI-3 review bot<br/>doing"] --> CI1["CI-1 Blacksmith runners<br/>VERIFY owed"]
    RB4["RB-4 header verdicts"] --> RB5["RB-5 solver"]
    RB5 --> RB6["RB-6 tile streams"]
    RB6 --> RB7["RB-7 report and apply"]
    RB7 --> RB8["RB-8 calibration"]
    RB8 --> RB9["RB-9 docs"]
    RB7 --> RC10["RC-10 open recovery work"]
    RB7 --> RC4["RC-4.2 corpus union"]
    R3["R-3 release 1.1"] --> R4["R-4 Python bindings"]
    R4 --> R5["R-5 JS/WASM bindings"]
    R3 --> RC5["RC-5 checksum footer<br/>1.2 material"]
    TE1["TE-1 tile encoding 3 is AV1<br/>doing"] --> OD["OD-T1 key plane,<br/>decode order to depth"]
    IV1["IV-1 I2S version<br/>packed in VERSION"]
    TE2["TE-2 tile encodings 4, 5:<br/>UASTC, XUASTC<br/>doing"] --> OD2["OD-T2 mip levels<br/>OD-T3 XUASTC footprint"]
    VL1["VL-1 parallel diagnostic log"]
    ZC["ZC-1..5 span read path"]
    BL["BL-1..7 backlog"]
```

RB-4 to RB-9 run in order, each ending at its gate. R-3 waits on the
maintainer's review of the 1.1 revision (R-3.1). VL-1, ZC and the backlog
touch different files from the recovery lane and can run beside it.

| Package | Purpose | Status |
|---|---|---|
| CI-3 | the review bot reviews every pull request; the plan moves to `development/` | `doing` (pull request #6) |
| TE-1 | tile encoding value 3 is `TILE_ENCODING_AV1`, a bare AV1 sequence, in place of AVIF | `doing` (pull request #7, stacked on #6); TE-1.4 `blocked` on OD-T1 |
| IV-1 | `ATTRIBUTES.VERSION` under `METADATA_I2S` is the I2S version packed as `major << 8 \| minor` (D27) | `doing` (pull request #8, stacked on #7) |
| TE-2 | tile encodings 4 and 5 are `TILE_ENCODING_UASTC` and `TILE_ENCODING_XUASTC`, bare GPU block-compressed streams | `doing` (pull request #6); TE-2.4 `blocked` on OD-T2, TE-2.5 on OD-T3 |
| CI-1 | every CI job on a pinned Blacksmith runner | `done`; VERIFY and the macOS gating owed |
| VL-1 | the validation layer's diagnostic sink is safe under a multithreaded writer | `todo` (P1, latent) |
| RB-4 to RB-9 | the recovery census grows its solver, tile streams, report, apply and calibration | `todo`; RB-5 to RB-7 wait on OD-R1, OD-R3, OD-R4, OD-R5 defaults |
| RC-4, RC-5, RC-10 | open items of the recovery work orders | `todo`; RC-10.3 and RC-10.8 are Iris-Slide's half |
| R-3 | release 1.1 | `blocked` on the maintainer's review of the revision (R-3.1) |
| R-4, R-5 | Python and JavaScript/WebAssembly bindings | `todo`, after R-3 (D15) |
| R-6 | an expectations header for the 1.1 witness | `doing`: the test exists, its record is owed |
| ZC-1 to ZC-5 | the read path as a const lens (D14) | `todo` |
| BL-1 to BL-7 | carried forward, none gating the release | `todo` |

---

## CI-3 — The Aptix governance review bot reviews every pull request `doing`

Every non-draft pull request from a branch of this repository is reviewed by
the Aptix review bot against `.github/review/rubric.toml` (D24). Design,
sources and the bot's pinned commit:
[analysis/blacksmith_ci_and_review_bot_2026-11-24.md](analysis/blacksmith_ci_and_review_bot_2026-11-24.md).
Branch `feature/ReviewBot_CI-3`, pull request #6.

- [x] CI-3.1 **Red first.** `cmake/check_review_configuration.cmake`, run by the `spec` job, fails on any change that would let `review.yml` run a pull request's code, widen a job's token, move a secret or unpin an action. Evidence: exit 1 at the test's own commit, naming the four missing files. [d68abca](https://github.com/Aptix-Bio/Iris-File-Extension/commit/d68abcaffb499cc1d4249265e18c97c5308b1a33)
- [x] CI-3.2 **Green.** The workflow, rubric, CODEOWNERS and pull request template added; the check exits 0. actionlint 1.7.12 reports only the three shellcheck notes `main` already had; `tools/runner_labels_lint.py` and `tools/portability_lint.py` pass. [83c4fe4](https://github.com/Aptix-Bio/Iris-File-Extension/commit/83c4fe42b4f124ac5305b5b38d18a81ec4f8138c)
- [x] CI-3.3 **The rubric loads in the pinned bot.** `load_rubric` and `code_owners` at the pinned governance commit (Python 3.15.2, its hash-pinned requirements): 13 items, 12 blocking and Documentation advisory; `[trace]` on TASKS.md and MIGRATION.md; the owners on CODEOWNERS' catch-all line; all 13 reference files present; MIGRATION.md was then dropped from them to cut each review by two-thirds, leaving 12. [83c4fe4](https://github.com/Aptix-Bio/Iris-File-Extension/commit/83c4fe42b4f124ac5305b5b38d18a81ec4f8138c)
- [ ] CI-3.4 **VERIFY:** the first review run, on the first pull request
      opened after this merges (`pull_request_target` runs the default
      branch's workflow, so this pull request gets none).
- [ ] CI-3.5 **Required check.** Making `2. Update PR from Agent` a required
      status check on `main` is a repository setting for the maintainer.
- **CI-3.6** — the root planning documents become `development/` (D25):
  `MIGRATION.md`, `TASKS.md` and `architecture.md` retired into
  ARCHITECTURE, WBS, RTM, PIPELINE and `analysis/`, every work-item ID kept,
  every reference updated, and the rubric's reference set and `[trace]`
  pointed at the new files [Process_01]
  - [x] CI-3.6.1 **Nothing dropped.** The move's checker (`python3 tools/check_planning_move.py .`) lists every work-item ID and every box of the three retired files at [005347e](https://github.com/Aptix-Bio/Iris-File-Extension/commit/005347ee3dd9ef802b967d795db17e564a7c8a27) and finds each in `development/`: 107 work-item IDs, the 44 open ones in WBS.md; 37 open boxes, all in WBS.md; 79 ticked boxes, all in `development/`. A WBS copy with RB-5 deleted fails it, naming RB-5. [47cd97c](https://github.com/Aptix-Bio/Iris-File-Extension/commit/47cd97c65c26e897e5ead97af2a6616e77cea05e)
  - [x] CI-3.6.2 **References and rubric.** `grep` finds the retired files named only in provenance notes and the dated records; the rubric sends 15 reference files (211,205 characters, from 446,068 with MIGRATION.md), and `[trace]` names WBS.md ticks and RTM.md rows; `cmake -P cmake/check_review_configuration.cmake` exits 0, and its new `[trace]` file check exits 1 on the base rubric with MIGRATION.md deleted, which the base check passed. [47cd97c](https://github.com/Aptix-Bio/Iris-File-Extension/commit/47cd97c65c26e897e5ead97af2a6616e77cea05e)
  - [x] CI-3.6.3 **The pinned bot loads it.** `load_rubric` at the pinned governance commit: 13 items, `[trace]` on WBS.md and RTM.md, all 15 reference files present; the 14 Mermaid diagrams of `development/` render (mermaid-cli). [47cd97c](https://github.com/Aptix-Bio/Iris-File-Extension/commit/47cd97c65c26e897e5ead97af2a6616e77cea05e)
  - [ ] CI-3.6.4 **VERIFY:** the maintainer reviews `development/`.
- [x] CI-3.7 **Nesting at most three levels.** `cmake/check_review_configuration.cmake` (five levels at the governance checkout's checks, four in the per-line checks and the CODEOWNERS check) and `tools/check_planning_move.py` (four at the open-box check) move their inner loops into helper functions, as development/style_guide.md, Control flow, requires. The check now counts problems in a global property, so a helper at any depth reports one. Evidence: nesting at most 3 in both (a count of if/foreach/while depth; 5 before); of 18 mutations of review.yml, CODEOWNERS and the rubric, the base check let one pass (`actions/checkout@v7`) and the refactored check fails all 18; the move's checker prints the same report, and fails on a WBS copy without RB-5. [64a4ee0](https://github.com/Aptix-Bio/Iris-File-Extension/commit/64a4ee0e410c8de2ff3132ea19313fe8e219a891)
- [x] CI-3.8 **Red first: one command per `run:` step.** The check requires every `run:` step of `review.yml` to be one `cmake [-D...] -P base/cmake/<name>.cmake` command (development/ci_runners.md, "Workflows are simple; CMake does the work"); it exits 1 on the four steps that call Python or `echo` directly (`transcript-key`, `review`, `publish`, the fork notice). Evidence: exit 1 at the test's own commit, 4 problems, one per step. [78f7023](https://github.com/Aptix-Bio/Iris-File-Extension/commit/78f702328dbbc69d514fe0b73955915342f2cf09)
- [x] CI-3.9 **Green.** `cmake/review_bot.cmake` runs one command of the bot (`-DSTEP=transcript-key`, `review`, `publish` or `fork-notice`) with the values the step's `env:` sets, refuses a missing value or one holding `;` (a CMake list separator) by name, and fails when `review` writes no `verdict.json` or an empty one; each `run:` step of `review.yml` is one `cmake -P`, and `fork-skipped` checks out the default branch's commit (contents: read) for its script. The check also requires each script a step runs to exist. `review.yml`'s comments give the bot's 128,000 output tokens (`MAX_TOKENS` at the pinned commit) and why 30 minutes bound them, and say why the settings check is each job's second step. Evidence: the check exits 0; 21 mutations each exit 1; `review_bot.cmake` against a stand-in bot passes each command's arguments and exits 1 on each of its six refusals; actionlint 1.7.12 reports nothing in `review.yml`; ctest 25 of 25 (Release, AppleClang); `--validate`, `--check`, both lints and the move's checker pass. [83b113d](https://github.com/Aptix-Bio/Iris-File-Extension/commit/83b113dabefb6f6b77d040e043b8baf31e738d18)
- [ ] CI-3.10 **The merge-verification job.** The bot's `verify` command ticks the description's human-verification line from a job that waits on a `merge-verification` environment, as governance's own workflow runs it. IFE's rubric sets `[verification] allow_author` but no rubric item requires the job, and the environment is a repository setting for the maintainer, so the job is not added until the environment exists (`todo`).

## CI-1 — Every CI job runs on a pinned Blacksmith runner `done`; VERIFY owed

Decided 2026-11-24 (D23): CI runs on Blacksmith runners, a label change kept
minimal so the workflows stay simple. The label table and its sources are in
[analysis/blacksmith_ci_and_review_bot_2026-11-24.md](analysis/blacksmith_ci_and_review_bot_2026-11-24.md).

- [x] CI-1.1 **Red first.** `tools/runner_labels_lint.py` (run by the `spec` job) fails on a GitHub-hosted label, a label Blacksmith does not document, or a `-latest` label: 44 findings on the base [136dc1c](https://github.com/Aptix-Bio/Iris-File-Extension/commit/136dc1ccb5e23e9f7554f2867786a4cb1c605ec8), exit 1; exit 0 after the move. [79a2fb7](https://github.com/Aptix-Bio/Iris-File-Extension/commit/79a2fb79a836a9b7e547aab3101786c2f90ed83d)
- [x] CI-1.2 **Bounded.** Every job has `timeout-minutes`; `CMAKE_BUILD_PARALLEL_LEVEL` replaces the bare `-j` with the runner's vCPU count (2 on Linux and Windows, 6 on macOS). [79a2fb7](https://github.com/Aptix-Bio/Iris-File-Extension/commit/79a2fb79a836a9b7e547aab3101786c2f90ed83d)
- [ ] CI-1.3 **VERIFY:** every job green on Blacksmith. First run (45429158071,
      2026-11-24): every job started on a Blacksmith runner; spec and document
      green; the CMake, Bazel, Emscripten and big-endian builds failed at
      configure or compile because Iris-Headers' `main` then lacked the
      `IrisSlide` rename. Since then, run 45456544767 on pull request #6
      (2026-11-24) passed all 13 jobs on Blacksmith; the maintainer ticks this.
- [ ] CI-1.4 **macOS last.** A macOS minute bills as twenty 2-vCPU Linux minutes
      (same page, FAQ); gating the macOS legs on the others (`needs:`) waits for
      the per-platform reusable workflow decided 2026-11-24 (unified
      workflows whose steps are CMake scripts, so a step runs the same
      locally and in CI).
- [x] CI-1.5 Record in `development/WBS.md` and `development/RTM.md` once those files are committed: done by CI-3.6, where CI-1 is this section and RTM row Pipeline_01. [47cd97c](https://github.com/Aptix-Bio/Iris-File-Extension/commit/47cd97c65c26e897e5ead97af2a6616e77cea05e)

## CI-2 — The big-endian job runs on Blacksmith without Docker `done`

The `big-endian (s390x)` job failed in 22 seconds on Blacksmith because the
binfmt_misc registration for s390x did not hold inside the runner's microVM.
The job now cross-compiles with `g++-s390x-linux-gnu` and runs every test under
`qemu-s390x` through `CMAKE_CROSSCOMPILING_EMULATOR`
(`cmake --workflow --preset big-endian`). Cause and evidence:
[analysis/blacksmith_ci_and_review_bot_2026-11-24.md](analysis/blacksmith_ci_and_review_bot_2026-11-24.md).

- [x] CI-2.1 **The binaries are big-endian.** `ife_host_byte_order_tests`: the test binary is an ELF with `EI_DATA=2` and `e_machine=22` (S390); emulated it prints `compiled for big; memory layout big` and exits 0; the same source compiled natively exits 1. [3c5560b](https://github.com/Aptix-Bio/Iris-File-Extension/commit/3c5560b0b8e561785ec7a963d167661706e764dc)
- [x] CI-2.2 **Local green.** The preset passes 26 of 26 tests in an Ubuntu 24.04 arm64 container (CMake 3.28.3, 2026-11-24). [3c5560b](https://github.com/Aptix-Bio/Iris-File-Extension/commit/3c5560b0b8e561785ec7a963d167661706e764dc)
- [x] CI-2.3 **VERIFY:** `big-endian (s390x)` green on pull request #1, run 45447197501, job 138634119321: 26 of 26 tests under emulation in 2 min 46 s. [3c5560b](https://github.com/Aptix-Bio/Iris-File-Extension/commit/3c5560b0b8e561785ec7a963d167661706e764dc)
- [x] CI-2.4 **Downstream.** Green in run 45448367106 at [bcf1eb6](https://github.com/Aptix-Bio/Iris-File-Extension/commit/bcf1eb6176604c598bfd6ee4da04c75085ff280e)
      (job 138638305129, `Iris-Slide builds against this branch`, 3 min 54 s,
      on PR #1, 2026-11-24; every job in that run passed). Run 45447197501 had
      failed this job at configure with "Could not find nasm": Iris-Slide
      merged its vcpkg build (Iris-Slide cc17fb3) between run 45440640914 and
      that one, and vcpkg's aom port needs NASM from the host. The job's apt
      line now adds `nasm` and `pkg-config`, the tools Iris-Slide's own
      `_build.yml` installs.

## TE-1 — Tile encoding value 3 is `TILE_ENCODING_AV1` `doing`

Decided 2026-11-25 (D26): the AVIF tile encoding is removed and value 3 of
`tile_encodings` becomes `TILE_ENCODING_AV1`. The tile byte-stream is a bare
AV1 sequence in the Low Overhead Bitstream Format (AV1 specification §5.2,
https://aomediacodec.github.io/av1-spec/, read 2026-11-25), with no container:
one shown frame per focal plane, and a single-plane tile is one key frame. No
file in use carries the AVIF tile encoding, so the value is reassigned in
place instead of deprecated. `IMAGE_ENCODING_AVIF` (associated images) stays.
Specification: `spec/ife_constants.json` and `spec/ife_spec.adoc`, AV1 Tile
Byte-Streams. [Wire_06, Wire_07]

- [x] TE-1.1 **Red first.** The round-trip and conformance tests name `TILE_ENCODING_AV1`; the build fails with "no member named 'TILE_ENCODING_AV1' in 'Iris::File::constants::TileEncodings'" (`ife_blocks_tests.cpp:1243`, `ife_validation_tests.cpp:157`). [50230d5](https://github.com/Aptix-Bio/Iris-File-Extension/commit/50230d55f58aa35445189b4711b540f7c04ba55c)
- [x] TE-1.2 **Green.** The member is renamed and defined in the JSON and the specification text; the witness key is renamed by hand (the refresh tool refuses a break) and `tests/wire/README.md` records the break; the generated sources and documents are rebuilt; the example takes the name from the generated `member_name`. ctest 25 of 25 (Release) and 25 of 25 under ASan and UBSan; `--validate`, `--check`, both lints and `spec/build_document.sh` pass. [abd8082](https://github.com/Aptix-Bio/Iris-File-Extension/commit/abd80828ca6cfe2992fa7ed797e4918feadd622d)
- [ ] TE-1.3 **VERIFY:** the maintainer reviews the AV1 clause of the specification.
- [x] TE-1.5 **The exception is recorded.** Value 3 is a member of the ratified 1.0 group, so its reassignment breaks append-only; the code owner sanctions it (2026-11-25) because IFE has no deployed corpus. `tests/wire/README.md`, D26, the 1.1 revision summary and CONTRIBUTING.md's absolutes record it; `tools/refresh_witness.py` has no path that records a break, so the witness key stays renamed by hand. Evidence: `--validate` and `--check` pass and `tools/refresh_witness.py` reports the witness unchanged; ctest 25 of 25 (Release, AppleClang). [7afa979](https://github.com/Aptix-Bio/Iris-File-Extension/commit/7afa9797cb1a3dc3301ab73681a193806218de4c)
- [ ] TE-1.4 **Which plane is the key (in-focus) frame, and the mapping of decode order to focal depth.** `blocked` on OD-T1 (ARCHITECTURE.md §1.1). No key-plane field and no frame-to-depth ordering rule is added until it is decided; the specification marks both open.

## IV-1 — `ATTRIBUTES.VERSION` under `METADATA_I2S` is the I2S version, packed `doing`

Decided 2026-11-25 (D27): the u16 `ATTRIBUTES.VERSION` of a block declaring
`METADATA_I2S` holds the Iris Interoperability Specification version as
`major << 8 | minor` (I2S 0.1 is 0x0001, I2S 1.0 is 0x0100); zero is
unversioned, which a reader already takes as free text (D17). The rule is
prose about a value, not a wire fact, so `tests/wire/witness.json` is
unchanged. Specification: `spec/ife_fields.json` (VERSION),
`spec/ife_constants.json` (`METADATA_I2S`) and the ife-attributes clause of
`spec/ife_spec.adoc`. [Wire_08]

- [x] IV-1.1 **Red first.** `tests/generator/test_attributes_version.py` (ctest `ife_generator_attributes_version`) requires the packing as a "shall" in the schema and the narrative, with the unpacking and both examples, in the emitted tables, and off the wire: 25 of 26, the new test failing with 14 FAIL lines. `ife_builder_layered_tests` round-trips `METADATA_I2S` at VERSION 0x0001 and reads back both halves; it records behaviour the Builder already had and was green. [d9acd91](https://github.com/Aptix-Bio/Iris-File-Extension/commit/d9acd91b5ab6b727728521cf24500ba05fd4661d)
- [x] IV-1.2 **Green.** The VERSION and `METADATA_I2S` descriptions, the 1.1 revision summary and the ife-attributes clause state the rule; ctest 26 of 26 (Release); `--validate`, `--check` (no drift, witness not stale, so no refresh), both lints and `spec/build_document.sh` pass, and the rendered clause shows `major << 8 | minor`. [86ffcea](https://github.com/Aptix-Bio/Iris-File-Extension/commit/86ffceaa8f466e5dffe168629baedf4df3506ccb)
- [ ] IV-1.3 **VERIFY:** the maintainer reviews the ife-attributes clause.

## TE-2 — Tile encodings 4 and 5 are `TILE_ENCODING_UASTC` and `TILE_ENCODING_XUASTC` `doing`

Decided 2026-11-25 (D28): two GPU block-compressed tile encodings follow
`TILE_ENCODING_AV1` in the 1.1 group, so a reader can transcode a tile to the
block format its GPU samples (ASTC 4x4 or BC7) instead of decoding it to
pixels. Each tile byte-stream is a bare codec stream with no KTX2 or other
container: under `TILE_ENCODING_UASTC` (4), one Zstandard frame (RFC 8878
§3.1.1) of the tile's 16-byte UASTC LDR 4x4 blocks in raster order; under
`TILE_ENCODING_XUASTC` (5), one XUASTC LDR bitstream (XUASTC LDR
Specification v1.0, stream version 1). One plane and one mip level each. The
sources are cited in D28, read 2026-11-25. Specification:
`spec/ife_constants.json` and `spec/ife_spec.adoc`, UASTC Tile Byte-Streams,
XUASTC Tile Byte-Streams, Mip Levels of Block-Compressed Tiles. Encoding and
decoding are Iris-Slide's; the encoder's size parity with JPEG is an
Iris-Slide requirement. [Wire_09, Wire_10, Wire_11]

- [x] TE-2.1 **Red first.** The round-trip test loops over AV1, UASTC and XUASTC and the conformance test accepts both new members; the build fails with "no member named 'TILE_ENCODING_UASTC' in 'Iris::File::constants::TileEncodings'" (`ife_blocks_tests.cpp:1240`, `:1241`). [f91d5a4](https://github.com/Aptix-Bio/Iris-File-Extension/commit/f91d5a493b9a1230d805ea3b2c64b8440bcdd68c)
- [x] TE-2.2 **Green.** Both members are defined in the JSON's 1.1 group and in the specification text; the witness is refreshed with `tools/refresh_witness.py` (an addition); the generated sources and documents are rebuilt; the Builder's `append_tile` comment names both. ctest 26 of 26 (Debug) and 26 of 26 under ASan and UBSan; `--validate`, `--check`, determinism under two seeds, both lints and `spec/build_document.sh` pass.
- [ ] TE-2.3 **VERIFY:** the maintainer reviews the UASTC, XUASTC and mip-level clauses of the specification.
- [ ] TE-2.4 **Where a block-compressed stream records its mip levels.** `blocked` on OD-T2 (ARCHITECTURE.md §1.1). The specification defines level 0 alone until it is decided.
- [ ] TE-2.5 **XUASTC's block footprint and bitstream stability.** `blocked` on OD-T3. The specification takes the footprint from the bitstream and marks both open.

## VL-1 — The validation layer's diagnostic sink under a multithreaded writer `todo` (P1, latent)

`ValidationHooks::diagnostic` is a caller-owned `std::string*` attached once at
writer creation, so two concurrent failing stores assign it at once: undefined
behaviour, and a clear-then-read caller attributes another thread's message to
its own store. Decided 2026-09-15 (D20): a parallel log whose entries are
claimed by compare-and-swap. Latent: no shipped path attaches the layer to a
parallel encoder yet. The defect, its trace and the reference implementation:
[analysis/validation_sink_race_2026-09-15.md](analysis/validation_sink_race_2026-09-15.md).
[Validate_02]

- [ ] VL-1.1 **Replace the sink with a parallel log.** A caller-owned, fixed-capacity
      buffer; a writer claims `offset..offset+len` with a `compare_exchange_weak` loop
      that computes the end first and **refuses** (without moving the head) when the
      entry does not fit, counting the refusal in an atomic dropped counter; then
      copies its message into the claimed range. Messages are already string literals
      emitted by the generator, so the failure path still allocates nothing. CAS is
      used because `fetch_add` cannot refuse a claim, and an entry that overflows
      would leave unwritten bytes inside the readable range (FastFHIR's LOG-1).
- [ ] VL-1.2 **Add a caller-owned `std::atomic<uint64_t>* failures`** beside the sink,
      incremented on every failing check, so a caller can detect that a check
      fired without the clear-then-read pattern. Remove that pattern from the docs.
- [ ] VL-1.3 **Fix it in the emitter, not the generated file.** The struct and its
      comment, `__report` and the doc-comment example are emitted by
      `generator/emit/cpp.py` (re-locate with `grep -n diagnostic`); regenerate,
      and update `examples/validation_layer.cpp`, which assigns
      `hooks.diagnostic = &why`.
- [ ] VL-1.4 **Readers:** document that reading the log is valid once writers have
      stopped (or add a committed watermark advanced after each copy). Pick one; say
      which in the header.
- [ ] VL-1.5 **This is an ABI change** to `ValidationHooks` (the IFE header says the
      call site was emitted early precisely so the ABI would not move later). Record
      it in ARCHITECTURE.md §1, amending D20.
- [ ] VL-1.6 **Test under ThreadSanitizer:** one hooks struct, N threads each storing a
      spec-violating block through it, a log sized to overflow partway. Assert no
      TSan report, every line read back is a complete message, no NUL bytes, and
      `lines + dropped == failures == N × stores`.

**Acceptance:** VL-1.6 green under `-fsanitize=thread` in CMake and Bazel;
`examples/validation_layer.cpp` still runs; the wire witness unchanged (the
layer emits no wire constant).

## RB — The recovery rebuild: the census engine `todo`

The recovery engine was removed on 2026-09-28 so that it could be rebuilt on
FastFHIR's census → branch solver → transactional apply (D13). RB-0 to RB-3 are
done (closed table); the census exists. In order, each ending at its gate;
every new test is registered in both `tests/tests.cmake` and `tests/tests.bzl`,
and every gate includes the ASan build. Plan, diagrams and the removed
engine's refinements and where each lives now:
[analysis/recovery_rebuild_2026-09-28.md](analysis/recovery_rebuild_2026-09-28.md).
[Recovery_02, Recovery_03, Recovery_04, Recovery_05]

- [ ] **RB-0.d — False anchors on real tiles.** RB-0 (d) measured false-anchor
  rates on proxy JPEG tiles of synthetic tissue; the same measurement on a real
  slide's JPEG and AV1 tiles is still owed.
- [ ] **OD-R1, OD-R3, OD-R4, OD-R5 — decisions owed** (ARCHITECTURE.md §1.1).
  Work proceeds on the defaults; RB-5 to RB-7 change if the maintainer chooses
  otherwise.
- [ ] **RB-4 — The header and the trusted extent.** The version and the extent
  are decided first. Each header field gets a verdict:
  - MAGIC and RECOVERY are constants, restored only when enough blocks show
    the bytes are an Iris file;
  - EXTENSION_MAJOR/MINOR must be one of the known versions;
  - FILE_SIZE must equal the file's size;
  - FILE_REVISION has no witness, so it is reported and never written;
  - TILE_TABLE_OFFSET and METADATA_OFFSET are ordinary census slots.

  **Tests:** the removed root cases (magic within the flip budget; beyond it,
  and `NotAnIrisFile` writes nothing); every single flip in the header bytes.
- [ ] **RB-5 — The solver, for blocks.**
  - The cycle: candidates → significance radius → tree → components → accept →
    decide.
  - Arrays go through `derive` (STRIDE, COUNT, VersionSkew).
  - **Tests carried from the removed suite:** one witness damaged per class;
    both witnesses lost, recovered from the predecessor's extent; a wrong-type
    reading (FastFHIR's F01 shape: METADATA's slot flipped onto an innocent
    block; this is where RC-8's tag consensus lives now); a contended repoint
    is never confident; generational holes recover from the root; damage is
    never silent; wire offsets never wrap a bounds check; a superseded block is
    never attached (OD-R4).
- [ ] **RB-6 — Tile streams.**
  - A tile entry's candidates are framed streams whose TILE_INDEX matches
    exactly.
  - A lost TILE_OFFSETS array is rebuilt from frames: the anchor; SIZE = the
    next start − the anchor (a start is exact under any version, OD-R2); the
    index = TILE_INDEX. Z_PLANES may not exceed its layer's.
  - Unframed streams are reported and left unrepointed.
  - **Tests:** a surviving frame rebuilds its entry; unframed corruption is
    reported; a flipped entry never names another tile's stream.
- [ ] **RB-7 — Report and apply.**
  - Each verdict carries its planned writes.
  - `apply` writes into the target OD-R1 chooses: each group lands whole, is
    read back, or rolls back.
  - **Tests:** an out-of-range write aborts; recover → apply → recover reports
    everything Intact; Ambiguous and Unrecovered verdicts write nothing.
- [ ] **RB-8 — Bench and calibration.**
  - The bits→percent curve on the RB-0 fixture, counted in anchored units,
    with misattachment asserted zero and the clean control unchanged.
  - FastFHIR's constants (MAX_CHANCE 1/64, ACCEPT_ABS 32, ACCEPT_MARGIN 64,
    K_BEST 256, MAX_COMPONENT 8) start as priors and are re-measured here.
- [ ] **RB-9 — Docs.**
  - ARCHITECTURE.md §5 rewritten for the finished census, with the diagrams
    in the rebuild record.
  - CLAUDE.md "Recovery" keeps the paid-for rules and drops the removed
    engine's function names.
  - README "Recovering a Damaged File" returns, with an example the README
    gate compiles.

## RC — Open items of the recovery work orders `todo`

RC-1 to RC-10 were written against the engine removed on 2026-09-28; their
lessons reappear as tests of the census engine (RB-5 to RB-7). The items below
were open when it was removed. Where an item names a function of the removed
engine, the rule carries to the census code that replaces it. Full record:
[analysis/recovery_work_orders_2026-08-28.md](analysis/recovery_work_orders_2026-08-28.md).

- [ ] **RC-4.2 — Wire it.** CMake target + ctest, a Bazel `cc_test`, and the
      corpus harness unions the recovery *report* into its walk the way it
      already unions the scan types, since real corpus bytes are the only place
      the two-witness path is exercised at scale. Status (2026-08-28): the
      targets are green on both build systems; the union waits on RB-7's report.
- [ ] **RC-5.1 — Spec + wire.** A new spec block (tag `RECOVER_CHECKSUM`, a
      BYTE_ARRAY or dedicated BLOCK primitive) appended at EOF; the checksum
      covers the file's bytes up to the footer. A wire change: spec JSON +
      witness refresh (`tools/refresh_witness.py`), new generated handle.
      [Recovery_06]
- [ ] **RC-5.2 — Writer + reader.** The block writers stamp it on finalize;
      `validate_file_structure` verifies it as the last deep check. Old
      readers tolerate the trailing block (gap analysis classes post-last-entry
      runs as Trailing, benign).
- [ ] **RC-5.3 — Version story.** 1.1 is ratified; the footer is 1.2 material
      (append-only, new tag, no shipped field touched).
- [ ] **RC-5.4 — Tests.** Corpus fixtures + witness gain the footer; a
      one-byte flip anywhere in the file trips the footer while the
      structural validators may stay green. **Done when:** a byte flipped in
      tile payload, where no VALIDATION word exists, fails validation via the
      footer alone, and a clean file passes.
- [ ] **RC-10.2 — A content-level metric.** No IFE test compares recovered
      *content* against a clean denominator. Run `abstract_file_structure()`
      over the repaired file and diff the attribute tree, image list and tile
      table against the clean run. RC-10.1's probe found 13 fabricated leaves
      over 40 one-bit trials on the 1.0 witness, at most 6 in one trial, and did
      not classify them; asserting that each is an unwitnessed inline scalar
      (the documented boundary) is this item. One 2-bit trial produced 3, so
      "at most one leaf per flipped bit" is not a safe assertion.
- [ ] **RC-10.3 — Iris-Slide must write tile frames. (Cross-repo; lockstep.)**
      `TILE_INDEX` is the only identity witness in the format. Iris-Slide's
      encoder writes through the Builder (2026-09-30) and writes no frames on
      single-plane layers, so no real single-plane slide carries one. The IFE
      half is done (the corpus writer `tests/ife_corpus_writer_11.cpp` frames);
      frames are optional by design, so this is Iris-Slide's writer policy.
- [ ] **RC-10.4 — Block edges have no identity witness. (P2, design.)** A
      flipped offset landing on a valid block of the same type satisfies both
      witnesses at the new address. Measured on the 1.0 witness: METADATA's
      `ATTRIBUTES` slot moving between its three sibling `ATTRIBUTES` blocks.
      Closing it needs a third witness (a back-pointer, or an index field of
      the kind `TILE_INDEX` is for tiles): a spec change, append-only work.
- [ ] **RC-10.7 — Recovery's entry walkers bound the slot's START, not its
      width.** The removed engine's walkers capped entries at
      `(size - begin) / stride`, so a STRIDE narrower than the entry (reachable
      only under multi-bit damage) reads up to `entry_size - stride` bytes past
      EOF. `ArrayHeader::readable()` is the fix; the census's array walks
      (`derive`, RB-5) are held to it, with the sweep accounting unchanged.
- [ ] **RC-10.8 — Iris-Slide now refuses damaged slides at open. (Cross-repo.)**
      Iris-Slide's slide constructor calls `abstract_file_structure()` with no
      `validate_file_structure` first; since RC-10.1 that call throws on a
      structurally damaged file. Decide what a viewer does next: surface the
      diagnostic, or offer `Recovery` + `apply()` on a copy and reopen.

## R-3 — Release 1.1 `blocked` on the revision review

No tag has been cut on this repository and no GitHub release exists. R-3.4 is
done (closed table). Full record, including the normative gap the release
found and closed (the global tile indexing):
[analysis/release_work_orders_2026-08-18.md](analysis/release_work_orders_2026-08-18.md).
[Release_01]

- [ ] **R-3.1 — Ratify the revision.** `spec/ife_header.json` carries 1.1 as
  `"status": "draft"`. Set it ratified with an ISO date (`generator/validate.py`
  requires one). Held by decision (2026-08-19) until the migration is reviewed.
  **Done when:** `python3 -m generator --validate` exits 0 and the rendered
  cover no longer says draft.
- [ ] **R-3.2 — Refresh the wire witness** in the same change if the spec moved
  (`tools/refresh_witness.py`); `--check` fails until it does.
- [ ] **R-3.3 — Publish the document.** `spec/build_document.sh` produces the
  PDF and HTML; its three gates are the release gate, since Asciidoctor exits 0
  on a missing include.
- [ ] **Release** with the generated spec document as the published
      standard; ratification per Iris Developers process. Run `release.yml` by
      `workflow_dispatch` once first: only macOS arm64 has run it end to end,
      and the publish job has never executed.

## R-4 — Python bindings (pybind11) `todo` (P2)

Bind the `Abstraction::` surface behind its own CMake option, generate enum
values and field keys from `spec/*.json`, and add a CI job. Follows the tag
(D15). The binding analysis is kept in
[analysis/migration_phases_1_to_6.md](analysis/migration_phases_1_to_6.md),
Phase 6. [Release_02]

- [ ] **Python bindings (pybind11) over the abstraction layer.** Not a scope
      question: this repository owns the byte structure and its validation, so
      a Python caller manipulating that structure is inside its boundary. Bind
      `is_iris_slide_file`, `validate_file_structure`,
      `abstract_file_structure`, `generate_file_map` and the `Abstraction::`
      structs, each taking one `const FileAccessInfo&`. The module compiles the
      block layer itself (`IFE_HEADER_ONLY`), so the exported surface grows by
      zero symbols. Model it on FastFHIR's `ff.Memory` / `ff.Builder`.
- [ ] **Emitted binding surfaces** (stubs acceptable initially): field-key
      tables for Python and WASM/Emscripten (Phase 3; shared with R-5).

## R-5 — JS/WASM binding surface `todo` (P3)

[Release_03]

- [ ] **JS/WASM binding surface** — as above, and note the residency work is
      already done: `Iris::File::Window`'s Emscripten branch fetches byte ranges on demand,
      and the `wasm` CI job compiles it.

## R-6 — An expectations header for the 1.1 witness `doing`

Every 1.1 field asserted present-and-correct on the 1.1 witness and absent on
the 1.0 witness. `tests/ife_v11_fixture.hpp` and `tests/ife_v11_witness_tests.cpp`
exist in the tree (since 257b960) and run in CTest; R-6's completion was never
recorded. [Wire_04]

- [ ] **R-6 — record the evidence**: check the test against R-6's **Done when**
  (every 1.1 field present on the 1.1 witness and absent on the 1.0 witness;
  re-introducing the frame-anchor bug turns a test red), then tick with the run.

## ZC — The read path as a const lens `todo`

D14: read-path metadata and entry access are `std::span<const T>` over static
or in-place storage. Record and numbers:
[analysis/zero_copy_read_path_2026-08-19.md](analysis/zero_copy_read_path_2026-08-19.md).
[Read_05]

- [ ] **ZC-1** Block layout tables in `generated_source/IFE_Blocks.hpp` exposed as
      `std::span<const T>` (generator change in `generator/emit/cpp.py`),
      never returned by value.
- [ ] **ZC-2** Read path (`IFE_Runtime.cpp` / `IFE_Window.cpp`): no `std::vector`
      constructed per call when the source data is resident-addressable.
- [ ] **ZC-3** `AttributeSlice` / tile-entry access goes through spans over the block,
      without materialized `std::vector` layers.
- [ ] **ZC-4** Wire-format gate (if any) and the corpus harness measure against a
      Release build.
- [ ] **ZC-5** Benchmark parity: any public API change (e.g. a function now returning
      `std::span`) is noted for the companion benchmark repository.

## BL — Carried forward, none gating the release `todo`

Collected from the retired documents' "not blocking" and "deferred" notes.

- [ ] **BL-1 — Corruption tests.** `tests/ife_blocks_corruption_tests.cpp` (gates 4.2c) — v1 source for
      the expected failure behaviour was the retired `IrisCodecExtension.cpp`.
      Truncation, clobbered `VALIDATION`, clobbered `RECOVERY`, `stride == 0`,
      `count` overflow, and an offset cycle; each returns the specific `Check`
      code, never crashes and never reads out of bounds (under
      `-fsanitize=address,undefined`). The damage sweep (Read_02) covers the
      single-flip cases; these shapes are multi-byte.
- [ ] **BL-2 — Diagnostics generated from the JSON's normative clauses.** The
  validation layer (Phase 4.6) already emits its messages from the clauses;
  whether that closes this item is *(to confirm)*.
- [ ] **BL-3 — ThreadSanitizer and fuzzing.** No TSan preset or fuzz target
  exists; VL-1.6 adds the first TSan test.
- [ ] **BL-4 — A fixture with real AV1 tile bytes.** `TILE_ENCODING_AV1` (a
  bare Low Overhead Bitstream Format sequence, D26) appears in no committed or
  hosted bytes: a single-plane tile as one key frame, and a Z-stacked tile
  with one shown frame per plane. Rewritten 2026-11-25 from the AVIF fixture
  item, when value 3 became AV1.
- [ ] **BL-5 — The two deferred `TODO(draft)` markers** in `spec/ife_spec.adoc`:
  Equations 2.1/2.2 (MpP and Mc normalization) and Figure 2.7 (slide space).
  The prose states both rules; they want the 1.0 document's figure.
- [ ] **BL-6 — A Windows consumer gets no `IFE_IMPORT_API`**, so `IFE_EXPORT`
  is empty instead of `dllimport` on that side of the install. Functions link
  through the import library regardless; changing it is an export-contract task.
- [ ] **BL-7 — Pin Iris-Headers.** `IFE_IRIS_HEADERS_GIT_TAG` defaults to
  `main`, so a push there can change this build and the published document
  cannot be reproduced from a tag. Pin before a revision is ratified from a
  build.

---

## Closed work

Condensed: what each closed, when, and the evidence; the record named on the
right holds it in full, as written.

| ID | Closed | What, and the evidence | Record |
|---|---|---|---|
| Phase 1 | 2026-08-06 | Schema decisions settled (universal header, identification, version encoding, recovery-tag space, arrays, field types, inventory, write model, coexistence) | [phases](analysis/migration_phases_1_to_6.md) |
| Phase 2 | 2026-08-07 | `spec/ife_fields.json`, `ife_constants.json` and the stdlib-only validator (`--validate`) | [phases](analysis/migration_phases_1_to_6.md) |
| Phase 3 | 2026-08-18 | The generator, emitted C++, configure-time generation, `--check` parity (via XP-2); binding surfaces carried to R-4/R-5 | [phases](analysis/migration_phases_1_to_6.md) |
| Phase 4 (4.0–4.6) | 2026-08-12 | Decisions 4.0-A to 4.0-H; generated block I/O, validators and writers; the byte window; the semantic API; the validation layer; 100 % function coverage of `IFE_Blocks.cpp` and `IFE_Validation.cpp`; 14/14 ctest in Release and ASan+UBSan; the corruption-test item carried to BL-1 | [phases](analysis/migration_phases_1_to_6.md) |
| Phase 5 | 2026-08-12 | The AsciiDoc document pipeline: one `.adoc` per item, `spec/build_document.sh` with three gates, provenance in the document, the CI docs job | [phases](analysis/migration_phases_1_to_6.md) |
| Phase 6 (6.1–6.7) | 2026-08-18 | The hand-written v1 layer retired; the oracle on a hosted, digest-pinned snapshot; Iris-Slide's encoder on the generated API, with the `downstream` CI job; the shared memory arena in Iris-Headers; the corpus harness; bindings and the release carried to R-3 to R-5 | [phases](analysis/migration_phases_1_to_6.md) |
| XP-1 | 2026-08-18 | Append-only enforced by `tests/wire/witness.json` at `--validate` (Wire_01) | [cross-pollination](analysis/cross_pollination_2026-08-18.md) |
| XP-2 | 2026-08-18 | `--check` rebuilt on generated parity instead of byte equality | [cross-pollination](analysis/cross_pollination_2026-08-18.md) |
| XP-3 | 2026-08-18 | The generator determinism test (Wire_02) | [cross-pollination](analysis/cross_pollination_2026-08-18.md) |
| XP-4 | 2026-08-18 | The dangling-view test `ife_lifetime_tests`, under ASan, red-greened both ways (Read_04) | [cross-pollination](analysis/cross_pollination_2026-08-18.md) |
| R-1 | 2026-08-18 | The corpus harness: `ife_corpus_tests` compares declared and reached block types both ways (Corpus_01) | [release](analysis/release_work_orders_2026-08-18.md) |
| R-2 (R-2.1, R-2.2) | 2026-08-19 | The reachable corpus gaps closed: 18 of 18 block types; CIPHER resolved by isolation in its own fixture | [release](analysis/release_work_orders_2026-08-18.md) |
| R-3.4 | 2026-08-19 | `release.yml` builds four archives with the specification and `SHA256SUMS`; `tools/release_guard.py` refuses a draft; `tools/package_smoke/` links the installed package | [release](analysis/release_work_orders_2026-08-18.md) |
| RC-1 (1.1–1.4) | 2026-08-28 | Two-witness reconciliation, the Hamming ranker and `apply()` in the removed engine | [recovery work orders](analysis/recovery_work_orders_2026-08-28.md) |
| RC-2 (2.1–2.4) | 2026-08-28 | The extent model, the containment rule and the gap sweep (`GapClass`, kept in `IFE_Recovery.hpp`) | [recovery work orders](analysis/recovery_work_orders_2026-08-28.md) |
| RC-3 (3.1–3.6) | 2026-08-31 | Orphaning tests: clean-stream zero damage, one witness per class, both witnesses lost, frame survival, never silent | [recovery work orders](analysis/recovery_work_orders_2026-08-28.md) |
| RC-4.1, RC-4.3, RC-4.4 | 2026-08-28 | The report, the threat-model boundary beside the API, byte-level machinery confined to translation units | [recovery work orders](analysis/recovery_work_orders_2026-08-28.md) |
| RC-6 | 2026-08-31 | FastFHIR's REC-19 lessons: attestation-driven walk, the loss made visible, the reapply loop, a parallel census | [recovery work orders](analysis/recovery_work_orders_2026-08-28.md) |
| RC-8 | before 2026-09-28 | Tag consensus in the removed engine; carried to RB-5's wrong-type test | [recovery rebuild](analysis/recovery_rebuild_2026-09-28.md) |
| RC-9 (9.1–9.3) | 2026-09-09 | Misattribution (FastFHIR REC-23): `TILE_INDEX` adjudication, demote-only exclusivity, the frame path's bound | [recovery work orders](analysis/recovery_work_orders_2026-08-28.md) |
| RC-10.1 | 2026-09-09 | The read path checks what `validate()` checks (D12): 14,475 single-site damages under ASan+UBSan, zero errors (Read_01) | [recovery work orders](analysis/recovery_work_orders_2026-08-28.md) |
| RC-10.5 | 2026-09-09 | `-Wall -Wextra -Wswitch` (`/W4`) in both build systems, eight warnings fixed without suppression (D21) | [recovery work orders](analysis/recovery_work_orders_2026-08-28.md) |
| RC-10.6 | 2026-09-09 | `HoleCandidate::word` deleted, behaviour-neutral by A/B on the sweep | [recovery work orders](analysis/recovery_work_orders_2026-08-28.md) |
| RB-0 | 2026-09-28 | The premises measured: density and one parent hold on every writer; order is no witness; false-anchor rates on proxy tiles (real tiles: RB-0.d) | [recovery rebuild](analysis/recovery_rebuild_2026-09-28.md) |
| RB-1 | 2026-09-28 | The read-path damage sweep restored without recovery: 43,216 flips; a validate/abstraction disagreement found and fixed (Read_02) | [recovery rebuild](analysis/recovery_rebuild_2026-09-28.md) |
| RB-2 | 2026-09-28 | The generated reference table `reference_fields_view`: 16 edges, all exercised by the corpus | [recovery rebuild](analysis/recovery_rebuild_2026-09-28.md) |
| RB-3 | 2026-09-28 | One walk (`slots_of`) and the census: 47,744 flips, every point explained (Recovery_01) | [recovery rebuild](analysis/recovery_rebuild_2026-09-28.md) |
| Handle layer | 2026-09-30 | `Builder` and `Parser`, the `Iris::File::` namespace, the caller-owned layout; Iris-Slide's encoder moved onto the Builder with byte-identical output (D6 to D11) | [handle layer](analysis/handle_layer_2026-09-28.md) |

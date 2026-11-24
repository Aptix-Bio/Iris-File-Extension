# TASKS — Iris File Extension

Tickets filed from outside the recovery work orders. **The authoritative task list for
recovery is still `MIGRATION.md`** (`# ▶ RECOVERY WORK ORDERS`); this file holds items
that do not belong to that series. Re-verify every `file:line` with `grep` before
editing — line numbers drift.

---

## VL-1 — The validation layer's `std::string*` diagnostic sink is a data race under a multithreaded writer (P1, latent)

**Filed 2026-09-15** from FastFHIR, where the same design was ported for its conformance
layer (FastFHIR `TASKS.md` Block K, decision D4) and changed for exactly this reason.
Ryan's direction: IFE writes with multiple threads too, so the sink has to be a parallel
logger that threads drop entries into simultaneously, claiming space by CAS.

### The defect

`ValidationHooks::diagnostic` is a caller-owned `std::string*`
(`generated_source/IFE_Blocks.hpp:670`), and every failing check writes it through one
helper:

```cpp
// generated_source/IFE_Validation.cpp:25
if (__hooks != nullptr && __hooks->diagnostic != nullptr)
    *__hooks->diagnostic = __message;
```

`std::string::operator=` is not thread-safe. The hooks struct is attached **once, at
writer creation** (the header's own comment), so every thread that calls `store()` on
that writer shares the same `std::string`. Two concurrent failing stores assign it at
the same time: undefined behaviour, in practice a torn string, a double free during
reallocation, or a crash inside the layer. The layer is `noexcept`, so a crash there
takes the process down.

The documented usage contract fails under concurrency too, and fails silently:
"clear it before a store to detect whether one fired" (`IFE_Blocks.hpp`, the comment on
`diagnostic`). With several writers, thread A's clear erases thread B's message, and
thread A then reads B's message as its own. The `Status` is still correct because it is
returned per call, but the sink attributes diagnostics to the wrong store.

### Where the concurrency comes from

Iris-Slide encodes tiles on `hardware_concurrency()` workers and hands each stream to
`Builder::append_tile` after compression returns (since 2026-09-30; it used to claim
with its own `offset.fetch_add`), which claims lock-free from any thread. A writer that
validates `TILE_PIXEL_DATA` or the per-layer tables from those workers reaches
`__report` from many threads at once.

**Current exposure: latent.** In the checkouts read on 2026-09-15, Iris-Codec (now Iris-Slide) did not
reference `ValidationHooks`, so no shipped path attaches the layer to the parallel
encoder yet. The bug fires the first time one does, and nothing in the API warns
against it. `examples/validation_layer.cpp` and every test drive it from one thread,
which is why the suite is green.

### Fix

- [ ] VL-1.1 **Replace the sink with a parallel log.** A caller-owned, fixed-capacity
      buffer; a writer claims `offset..offset+len` with a `compare_exchange_weak` loop
      that computes the end first and **refuses** (without moving the head) when the
      entry does not fit, counting the refusal in an atomic dropped counter; then
      copies its message into the claimed range. Messages are already string literals
      emitted by the generator (`generator/emit/cpp.py:1810`, `:1923`), so the failure
      path still allocates nothing. Use CAS rather than `fetch_add`: `fetch_add` cannot
      refuse a claim, so an entry that overflows leaves unwritten bytes inside the
      readable range (the defect FastFHIR filed as LOG-1).
- [ ] VL-1.2 **Add a caller-owned `std::atomic<uint64_t>* failures`** beside the sink,
      incremented on every failing check, so a caller can detect "did anything fire"
      without the clear-then-read pattern. Remove that pattern from the docs.
- [ ] VL-1.3 **Fix it in the emitter, not the generated file.** The struct and its
      comment are emitted at `generator/emit/cpp.py:1026–1038`, `__report` at
      `:1846–1850`, and the usage example in the doc comment at `:1979`. Regenerate,
      and update `examples/validation_layer.cpp` (`:91`, `:107`, `:127`, `:146` assign
      `hooks.diagnostic = &why`).
- [ ] VL-1.4 **Readers:** document that reading the log is valid once writers have
      stopped (or add a committed watermark advanced after each copy). Pick one; say
      which in the header.
- [ ] VL-1.5 **This is an ABI change** to `ValidationHooks` (the IFE header says the
      call site was emitted early precisely so the ABI would not move later). Record
      it in `MIGRATION.md`.
- [ ] VL-1.6 **Test under ThreadSanitizer:** one hooks struct, N threads each storing a
      spec-violating block through it, a log sized to overflow partway. Assert no
      TSan report, every line read back is a complete message, no NUL bytes, and
      `lines + dropped == failures == N × stores`.

**Acceptance:** VL-1.6 green under `-fsanitize=thread` in CMake and Bazel;
`examples/validation_layer.cpp` still runs; wire witness unchanged (the layer emits no
wire constant).

**Reference implementation to compare against:** FastFHIR `include/FF_Conformance.hpp`
(`ValidationHooks::diagnostic` / `failures`), `src/FF_Builder.cpp`
(`_check_conformance`), and, once LOG-1 lands, `include/FF_Logger.hpp`.

---

## CI-1 — Every CI job runs on a pinned Blacksmith runner (done, owes its first green run)

**Filed 2026-11-24.** Ryan: "We should be using blacksmith for our CI runners".
GitHub-hosted jobs in Aptix-Bio's private repositories stopped starting over
billing that day; this repository is public, so its jobs still ran, but all CI
moves to Blacksmith. A label change, kept minimal under Ryan's rule of the same
day ("Simplified streamlined workflows are key"). Labels from
[Blacksmith's runner overview](https://docs.blacksmith.sh/blacksmith-runners/overview)
(read 2026-11-24).

| Job | Was | Now |
|---|---|---|
| `ci.yml` spec, downstream, sanitizers, document, wasm, big-endian; `tests` GCC and Clang; `bazel` Linux | `ubuntu-latest` | `blacksmith-2vcpu-ubuntu-2404` |
| `ci.yml` `tests` MSVC; `bazel` Windows | `windows-latest` | `blacksmith-2vcpu-windows-2025` (public beta; Visual Studio Build Tools 2022 in place of the full IDE) |
| `ci.yml` `tests` AppleClang and Xcode | `macos-latest` (macOS 26) | `blacksmith-6vcpu-macos-26` |
| `release.yml` verify, document, publish | `ubuntu-latest` | `blacksmith-2vcpu-ubuntu-2404` |
| `release.yml` package linux-x86_64, linux-arm64 | `ubuntu-22.04`, `ubuntu-22.04-arm` | `blacksmith-2vcpu-ubuntu-2204`, `-2204-arm` (deprecated by Blacksmith; kept for the glibc 2.35 floor) |
| `release.yml` package windows-x64 | `windows-2022` | `blacksmith-2vcpu-windows-2025` (the only Windows image offered) |
| `release.yml` package macos-universal | `macos-14` | `blacksmith-6vcpu-macos-15` with `MACOSX_DEPLOYMENT_TARGET=14.0` (no macOS 14 image; the target keeps the floor) |

- [x] CI-1.1 **Red first.** `tools/runner_labels_lint.py` (run by the `spec` job)
      fails on a GitHub-hosted label, a label Blacksmith does not document, or a
      `-latest` label. Evidence: 44 findings on the base `136dc1c`, exit 1; exit 0
      after the move. actionlint 1.7.12 reports only the three shellcheck notes
      the base already had (labels declared in `.github/actionlint.yaml`).
- [x] CI-1.2 **Bounded.** Every job has `timeout-minutes`; `CMAKE_BUILD_PARALLEL_LEVEL`
      replaces the bare `-j`, which under Makefiles starts every compile at once,
      with the runner's vCPU count (2 on Linux and Windows, 6 on macOS).
- [ ] CI-1.3 **VERIFY:** every job green on Blacksmith. First run (45429158071,
      2026-11-24): every job started on a Blacksmith runner; spec and document
      green; the CMake, Bazel, Emscripten and big-endian builds fail at configure
      or compile because Iris-Headers' `main` lacks the `IrisSlide` rename
      (`IrisSlideTypes.hpp: No such file or directory`), so they owe a green run
      after Iris-Headers#5 merges. The Bazel Windows leg reached `cl.exe` on the
      Build Tools, which shows MSVC was found.
- [ ] CI-1.4 **macOS last.** A macOS minute bills as twenty 2-vCPU Linux minutes
      (same page, FAQ); gating the macOS legs on the others (`needs:`) waits for
      the per-platform reusable workflow of Ryan's 2026-11-24 rule ("unified
      workflows with good Cmake subscripts").
- [ ] CI-1.5 Record in `development/WBS.md` and `development/RTM.md` once those
      files are committed (they exist only as untracked files on 2026-11-24).

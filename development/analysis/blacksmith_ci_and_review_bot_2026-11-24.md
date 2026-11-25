# CI-1 to CI-3: Blacksmith runners, the s390x job, the review bot

> **Record.** Written 2026-11-24 in TASKS.md at the repository root; moved here on
> 2026-11-24 when `development/` replaced the root planning documents (git history
> keeps the originals; the last commit that holds them is 005347e). Kept as written,
> with local paths, sibling-checkout paths and a quotation restated for a public
> repository. Open items are tracked in [WBS.md](../WBS.md) under the same IDs; their
> boxes here read `(open: WBS.md)`. Dated records keep the names they had (Iris-Codec
> is now Iris-Slide).

## CI-1 — Every CI job runs on a pinned Blacksmith runner (done, owes its first green run)

**Filed 2026-11-24.** Decided that day: CI runs on Blacksmith runners.
GitHub-hosted jobs in Aptix-Bio's private repositories stopped starting over
billing that day; this repository is public, so its jobs still ran, but all CI
moves to Blacksmith. A label change, kept minimal because the same day's
decision is that workflows stay simple and streamlined. Labels from
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
- (open: WBS.md) CI-1.3 **VERIFY:** every job green on Blacksmith. First run (45429158071,
      2026-11-24): every job started on a Blacksmith runner; spec and document
      green; the CMake, Bazel, Emscripten and big-endian builds fail at configure
      or compile because Iris-Headers' `main` lacks the `IrisSlide` rename
      (`IrisSlideTypes.hpp: No such file or directory`), so they owe a green run
      after Iris-Headers#5 merges. The Bazel Windows leg reached `cl.exe` on the
      Build Tools, which shows MSVC was found.
- (open: WBS.md) CI-1.4 **macOS last.** A macOS minute bills as twenty 2-vCPU Linux minutes
      (same page, FAQ); gating the macOS legs on the others (`needs:`) waits for
      the per-platform reusable workflow decided 2026-11-24 (unified
      workflows whose steps are CMake scripts, so a step runs the same
      locally and in CI).
- (open: WBS.md) CI-1.5 Record in `development/WBS.md` and `development/RTM.md` once those
      files are committed (they exist only as untracked files on 2026-11-24).

---

## CI-2 — The big-endian job runs on Blacksmith without Docker (done)

**Filed 2026-11-24.** After the move to Blacksmith (CI-1), the `big-endian (s390x)`
job failed in 22 seconds (run 45440640914, job 138610470770) before compiling
anything.

**Cause.** `docker/setup-qemu-action` ran `tonistiigi/binfmt --install s390x`,
which reported `installing: s390x OK` with `linux/s390x` supported. The same
action's platform listing a moment later showed only `linux/amd64` and
`linux/386`, and `docker run --platform linux/s390x alpine` then failed with
`exec /bin/sh: exec format error`: the kernel had no binfmt_misc handler for
s390x ELF files when the container started, so the registration did not hold on
the runner VM. Blacksmith runs each Linux job in a Firecracker microVM
([runner overview](https://docs.blacksmith.sh/blacksmith-runners/overview), read
2026-11-24). Its documentation says nothing about QEMU or binfmt_misc, and for
multi-platform Docker builds it recommends one native runner per architecture in
place of emulation
([Docker builds](https://docs.blacksmith.sh/blacksmith-caching/docker-builds),
read 2026-11-24). Why the registration is lost inside the VM is therefore not
documented; the log shows that it is.

**Fix.** The job no longer uses Docker or binfmt_misc. The host installs the
s390x cross compiler and QEMU user mode with apt (`g++-s390x-linux-gnu`,
`qemu-user`), and `cmake --workflow --preset big-endian` does the rest:
`cmake/toolchains/s390x-linux-gnu.cmake` sets the cross compilers and
`CMAKE_CROSSCOMPILING_EMULATOR` to `qemu-s390x -L /usr/s390x-linux-gnu`, so ctest
calls the emulator directly for every test binary. Generation and the corpus
fetch run at configure on the host. The whole suite runs emulated (26 tests),
which covers the bytes, blocks and window suites and the v1 oracle against the
corpus; a label filter was not needed because the emulated suite takes about
20 seconds. The compiler runs natively, which also retires the inline
workarounds for include paths that failed to resolve when the compiler itself
ran under QEMU. Actions in the job are pinned by commit SHA.

- [x] CI-2.1 **The binaries are big-endian.** `ife_host_byte_order_tests`
      (enabled by `IFE_HOST_BYTE_ORDER=big` in the preset) checks
      `std::endian::native` and the in-memory layout of a stored `uint32_t`.
      Evidence (Ubuntu 24.04 arm64 container on an Apple Silicon host,
      CMake 3.28.3, 2026-11-24): the test binary is an ELF with
      `EI_DATA=2` (big-endian) and `e_machine=22` (S390); emulated it prints
      `compiled for big; memory layout big` and exits 0; the same source compiled
      natively prints `compiled for little; memory layout little` and exits 1.
- [x] CI-2.2 **Local green.** The preset passes 26 of 26 tests in that container.
      The container host is arm64 where the runner is x86-64, and the container
      starts from `ubuntu:24.04` instead of Blacksmith's image, so the apt
      install and the job itself are proved only by the CI run.
- [x] CI-2.3 **VERIFY:** `big-endian (s390x)` green on PR #1 (run 45447197501,
      job 138634119321: 26 of 26 tests under emulation; the job took 2 min 46 s).
- [x] CI-2.4 **Downstream.** Green in run 45448367106 at [bcf1eb6](https://github.com/Aptix-Bio/Iris-File-Extension/commit/bcf1eb6176604c598bfd6ee4da04c75085ff280e)
      (job 138638305129, `Iris-Slide builds against this branch`, 3 min 54 s,
      on PR #1, 2026-11-24; every job in that run passed). Run 45447197501 had
      failed this job at configure with "Could not find nasm": Iris-Slide
      merged its vcpkg build (Iris-Slide cc17fb3) between run 45440640914 and
      that one, and vcpkg's aom port needs NASM from the host. The job's apt
      line now adds `nasm` and `pkg-config`, the tools Iris-Slide's own
      `_build.yml` installs.

---

## CI-3 — The Aptix governance review bot reviews every pull request (done; owes its first review run)

**Filed 2026-11-24.** Every non-draft pull request from a branch of this
repository is reviewed by the Aptix review bot against
`.github/review/rubric.toml`, the same design as Iris-Headers and Iris-Slide
use. The bot lives in the private Aptix-Bio/governance repository; a public
repository cannot name a private repository's action in `uses:` ("Access is
allowed only from private repositories",
https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/managing-github-actions-settings-for-a-repository,
read 2026-11-24), so each job checks the bot out at a pinned commit
(`eea566a60b93920050c30bfce901e250668186a1`) with `GOVERNANCE_READ_TOKEN` and
runs it from `./governance/review-bot`.

**Design.** `.github/workflows/review.yml` runs on `pull_request_target`
(the workflow and `github.sha` come from the default branch:
https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows,
read 2026-11-24) with `permissions: {}` and two jobs: `review` holds
`ANTHROPIC_KEY` with contents and pull-requests read only; `code-review` has
pull-requests write and never sees the key. Neither checks out, builds or runs
the pull request's head; the rubric, the documents it names and CODEOWNERS
are read from the default branch, so a pull request cannot change the rules it
is judged by (https://docs.github.com/en/actions/reference/security/securely-using-pull_request_target,
read 2026-11-24). A fork's pull request gets a notice saying why it is not
reviewed. Each job first names any missing secret
(`cmake/require_settings.cmake`) without printing its value. Actions are
pinned by full SHA (actions/checkout v7.0.1, actions/cache v6.1.0,
actions/upload-artifact v7.0.2, actions/download-artifact v8.0.2, each tag
read with `git ls-remote` 2026-11-24) and run on `blacksmith-2vcpu-ubuntu-2404`.
The rules the bot judges by are `development/style_guide.md` and
`development/ci_runners.md`, added here, with IFE's own planning and
specification documents.

- [x] CI-3.1 **Red first.** `cmake/check_review_configuration.cmake`, run by the `spec` job, fails on any change that would let `review.yml` run a pull request's code, widen a job's token, move a secret or unpin an action. Evidence: exit 1 at the test's own commit, naming the four missing files. [d68abca](https://github.com/Aptix-Bio/Iris-File-Extension/commit/d68abcaffb499cc1d4249265e18c97c5308b1a33)
- [x] CI-3.2 **Green.** The workflow, rubric, CODEOWNERS and pull request template added; the check exits 0. actionlint 1.7.12 reports only the three shellcheck notes `main` already had; `tools/runner_labels_lint.py` and `tools/portability_lint.py` pass. [83c4fe4](https://github.com/Aptix-Bio/Iris-File-Extension/commit/83c4fe42b4f124ac5305b5b38d18a81ec4f8138c)
- [x] CI-3.3 **The rubric loads in the pinned bot.** `load_rubric` and `code_owners` at the pinned governance commit (Python 3.15.2, its hash-pinned requirements): 13 items, 12 blocking and Documentation advisory; `[trace]` on TASKS.md and MIGRATION.md; the owners on CODEOWNERS' catch-all line; all 13 reference files present; MIGRATION.md was then dropped from them to cut each review by two-thirds, leaving 12. [83c4fe4](https://github.com/Aptix-Bio/Iris-File-Extension/commit/83c4fe42b4f124ac5305b5b38d18a81ec4f8138c)
- (open: WBS.md) CI-3.4 **VERIFY:** the first review run, on the first pull request
      opened after this merges (`pull_request_target` runs the default
      branch's workflow, so this pull request gets none).
- (open: WBS.md) CI-3.5 **Required check.** Making `2. Update PR from Agent` a required
      status check on `main` is a repository setting for the maintainer.

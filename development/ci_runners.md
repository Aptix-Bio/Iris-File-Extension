# CI Runners

How Iris-File-Extension uses CI runners (GitHub Actions on Blacksmith's
runners). Read it before writing or changing a workflow. Adopted 2026-11-24
(CI-3, [WBS.md](WBS.md)) from the runner rules the maintainer keeps for the Iris
repositories, so that the review bot judges a workflow change against rules
this repository carries. The jobs in `.github/workflows/ci.yml` that predate
these rules (CI-1, CI-2) move to them as their work items come up; a new or
changed job follows them now. Runner minutes are billed; a runner does only
work whose result is wanted, and never the
same work twice. Modular reuse and frequent cache hits are the two levers.

## When a runner runs

- **Only where a result is wanted.** Pull requests that are not drafts,
  release-candidate tags, release tags, and by hand. Never on a branch push;
  never for docs-only changes. A new commit cancels its PR's running run.
- **Cheapest first.** Gate the costly OS on the
  cheap ones passing (`needs:`; a macOS minute bills as twenty 2-vCPU Linux
  minutes, WBS.md CI-1.4), and gate a job on the jobs whose output it
  uses.
- **Prove it locally first.** A build or script runs locally, in a container
  for other OSes and architectures, before it spends a runner. Check that the
  local run can fail the way the runner would (an arm64 binary runs inside an
  "amd64" container on an Apple Silicon host); if it can't, assert the
  property directly. The container starts from what the runner image already
  has installed, not a bare image: a preinstalled host library a cross build
  can pick up by mistake fails only on the runner.

## Modular reuse

- **Workflows are modules, as code is.** One reusable workflow
  (`workflow_call`) per build, called once per OS; shared steps (fetching
  assets, signing in to a cloud) are a composite action every workflow calls.
  Never a second copy of steps that already exist.
- **Identical work runs once.** Work that doesn't depend on the platform (the
  assets, a toolchain archive covering every platform, generated data) is done
  by one job and handed on as an artifact, not repeated per platform.
- **Prebuilt over built.** Dependencies come prebuilt (apt, Homebrew, vcpkg
  binary archives); building one from source on a runner is a cache miss.

## Workflows are simple; CMake does the work

- **One workflow per purpose, few steps.** A `run:` step calls CMake: a
  configure, build or test command, or `cmake -P cmake/<name>.cmake`. Its
  logic lives in the script, which runs the same on every operating system's
  shell and locally, and is tested. Composite actions are for
  what needs an action (checkout, cache restore and save), with no shell logic
  beyond one call.
- **Every job on a pinned Blacksmith label.** `tools/runner_labels_lint.py`
  fails on any other label (CI-1.1).
- **Settings are checked first.** A job that needs a repository variable or
  secret names each missing one in its first step
  (`cmake/require_settings.cmake`), before an action fails without saying
  which setting to create. Where the script comes from a checkout, as in
  `review.yml`, the check is the step after that checkout, which needs no
  setting and so cannot fail for want of one.

## Caches

- **Cache everything restorable**: package archives, compiler cache, fetched
  sources, toolchain and engine downloads, assets.
- **Keys come from content** (a manifest's hash, the build files' hash, the
  package manager's commit and triplet), with a `restore-keys` prefix so a
  partial hit beats a miss. A key names anything the tool's own hash can't see
  (a deployment target passed through the environment).
- **Restore first, save as soon as filled.** Restore every cache at the top
  of the job; save each one in the step right after the one that fills it,
  before anything that can fail (a compile, a test). Caches of partial work
  (the compiler cache, fetched sources) save with `always()`. A cache saved
  only at job end is lost by the first failing test.
- **Same keys everywhere.** Every workflow uses the same key for the same
  content, so a release reads what CI saved.
- **Mind the scopes.** A cache belongs to the ref that saved it: seed the
  default branch by hand so pull requests and tags can read it, and delete a
  closed pull request's caches (unreadable by anything else, they crowd the
  seeded ones out of the repository's limit).
- **Keep the environment stable.** A variable a compiler cache hashes changes
  every key: set it only on the step that needs it.

## Robustness

- **Pin what can move.** Runner images (`ubuntu-24.04`, not `-latest`), tool
  and engine versions, action versions.
- **Bound everything.** Every job has a timeout; parallel builds have a job
  count that fits the runner's memory.
- **Fail loudly.** Check that each output exists and is within limits; an
  exit code of 0 with nothing written is a failure.

## Settings

The review bot (`.github/workflows/review.yml`, CI-3) needs two secrets,
`ANTHROPIC_KEY` (the model key) and `GOVERNANCE_READ_TOKEN` (read access to
the contents of Aptix-Bio/governance, where the bot lives). Both are
Aptix-Bio organization secrets; an administrator adds or rotates them under
the organization's or this repository's Settings, Secrets and variables,
Actions. A repository secret of the same name takes precedence over the
organization's ("the secret at the lowest level takes precedence":
https://docs.github.com/en/actions/reference/security/secrets, read
2026-11-24); this repository holds its own `GOVERNANCE_READ_TOKEN`. Each
review job's settings check, the step after its base checkout, names any that is missing
(`cmake/require_settings.cmake`) without printing a value.

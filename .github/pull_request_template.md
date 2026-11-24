<!--
Iris-File-Extension pull request. Fill each section from development/WBS.md and development/RTM.md,
which hold the work items and the requirements. Write "None" in a section that does not apply; never delete one. The review bot reads
this description as data and checks it against the diff (.github/review/rubric.toml).

"Agent review" at the bottom is not yours to tick: the review bot rewrites it on every run. A pull
request from a fork gets no model review; a maintainer reviews it.
Branch name: <kind>/<WorkPackage>_<n>, after a development/WBS.md work item, e.g.
feature/ReviewBot_CI-3.
-->

## Summary

<!-- Two or three sentences: what this delivers, and why. -->

## Work items

<!-- development/WBS.md work items completed or advanced, and the RTM rows touched, as numbered
there. Tick only steps whose evidence is on the same line, with a link to the commit that did the
work. -->

- **_ID_** — _title_
  - [x] _ID.n_ — _evidence_ — [_1234567_](https://github.com/Aptix-Bio/Iris-File-Extension/commit/_full-hash_)

## Specification impact

<!-- spec/*.json changes: each addition (version group, field, block, member) and the witness
refresh in this pull request; or "None: spec/ and tests/wire/witness.json unchanged". A change to
a shipped fact is a reviewed wire correction and says so. -->

## Decisions

<!-- Decisions made or changed, one line each, dated, with the consequence; recorded beside the
work item. "None" if none. -->

## Measurements

<!-- Numbers this change was decided on (throughput, latency, size), with the machine, the
compiler and the date; anything unmeasured marked (to measure). "None" if none. -->

## Test results

<!-- Per CI cell from the final run of this branch, with links. -->

| Cell | Result | Run |
|---|---|---|
| _spec documents and drift_ | _pass_ | _link_ |

## Red first

<!-- The run that failed before the change made it pass: the command, its failing output, and the
commit it ran at. A test that records existing behaviour says so. -->

## Failures fixed

<!-- Every test or CI failure met while doing this work that needed a change: symptom, root cause,
fix, and why the fix is right. "None" if none. -->

| Cell / step | Symptom | Root cause | Fix | Commit |
|---|---|---|---|---|

## Consumer impact

<!-- What Iris-Slide and any other consumer must change, and the downstream cell that shows
Iris-Slide builds against this change. A coordinated change names each consumer's pull request
and the order of the merges. "None: no consumer compiles anything this changes" if so. -->

- Iris-Slide: _none / what changes, its pull request_
- Iris-Headers: _none / the pull request this change needs first_
- Merge order: _none / e.g. Iris-Headers#n, then this, then Iris-Slide#n_

## Security impact

- New input parsed from a file or the network? _no / yes: where, and how every offset and length is bounded_
- New or upgraded dependency? _no / yes: name, version, licence, pinned hash_
- New permission, secret, variable, environment or token? _no / yes: which, and why_
- Patient data, slide metadata, secrets or local paths anywhere in the diff or logs? _no_

## AI authorship

<!-- Required. Which parts an AI agent wrote, with which tool and model, and what the human did
(directed, reviewed line by line, rewrote). -->

- Written by: _the maintainer / an AI agent (tool, model) / both — which parts_

## Agent review

<!-- Owned by the review bot. Hand edits are overwritten on its next run. -->
<!-- review-bot:begin -->
- [ ] **Traceability** — work items named; ticks carry evidence and a link to the work commit
- [ ] **StyleGuide** — `development/style_guide.md` followed
- [ ] **MinimumSurface** — no near-duplicates, no hand-written layout, nothing outside IFE's scope
- [ ] **TestsFirst** — new behaviour tested, shown red first; per-cell results; no test weakened
- [ ] **SpecLedger** — spec append-only; witness refreshed with it; generated output untouched
- [ ] **ByteOrderAndPortability** — little-endian on disk; s390x job and portability lint green
- [ ] **AbiAndConsumers** — IFE depends only on Iris-Headers; consumer impact stated; downstream green
- [ ] **PublicRepository** — nothing private named, no quotations, no local paths, no patient data
- [ ] **SecurityImpact** — section above filled in and matches the diff
- [ ] **Pipeline** — SHA pins, `permissions: {}`, untrusted input via `env:`, CMake steps, bounded jobs
- [ ] **DataProtection** — no slides, patient data, secrets, keys or binaries
- [ ] **Documentation** — Mermaid diagrams match the code; spec and README true (advisory)
- [ ] **AgentDisclosure** — AI authorship disclosed and specific

_Not yet reviewed._
<!-- review-bot:end -->

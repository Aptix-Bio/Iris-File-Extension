# Style Guide

Binding conventions for Iris-File-Extension's code and plans: the C++20
headers under `include/`, the runtime sources under `src/`, the generator
under `generator/`, the specification under `spec/`, their tests, and the
CMake and Bazel builds. The work items live in [TASKS.md](../TASKS.md) and
[MIGRATION.md](../MIGRATION.md), the layering in
[architecture.md](../architecture.md), the design notes in
[CLAUDE.md](../CLAUDE.md) and the wire contract in
[CONTRIBUTING.md](../CONTRIBUTING.md); where they are more specific than this
guide, they win. Adopted 2026-11-24 (CI-3, TASKS.md) from the style guide the
maintainer keeps for the Iris repositories, with the references to other
projects removed, so that the review bot (`.github/review/rubric.toml`) judges
a pull request against rules this repository carries.

## Principles

- **Minimum code surface.** Fewest functions, variables, branches and
  dependencies: each one is a risk vector. If two routines move similar data in
  similar ways, merge them into one with an enum or optional argument, and
  delete code to do it.
- **Concise over clever; explicit over implicit.** The obvious loop beats the
  one-liner. Comments explain *why*, never *what*.
- **Strongly typed, everywhere.** Types state the contract so the compiler, not
  the reader, enforces it (Typing, below).
- **Fail loudly.** A failing subsystem says so, with the offending value; it
  never returns empty and hopes the caller notices.
- **Self-cleaning.** Resources release themselves at scope exit.
- **Test first.** The test is shown red before the code that makes it green.
- **Measure before deciding.** Numbers, not assumptions; anything unmeasured
  is marked *(to measure)*.

## Planning

An agent's knowledge is a prior, not evidence. Versions, APIs, product plans
and standards change after a model's training, and a confident recollection
can be wrong.

After drafting a plan, and before presenting it as ready or starting work on
it, verify every external fact it depends on with an internet search against
primary sources (vendor documentation, release notes, the package registry,
the standard itself): versions, pins, commit SHAs, API shapes and flags; what a
product provides and its limits; prices, quotas and licences; the clauses of
standards cited (the C++ standard, the ICC specification).

Record each check beside the claim it supports: the source URL and the date it
was read. A claim that could not be verified stays marked *(to confirm)* and is
never presented as fact. When a search contradicts the plan, fix the plan and
say what changed and why. Facts internal to the repository (code, history,
documents) are verified by reading them, not by searching.

## The Specification and Generated Code

The format is defined by `spec/*.json`, and the C++ that lays out its bytes is
generated from them (`spec/README.md`, `generator/README.md`):

- **The JSON is the one source of every wire fact.** An offset, a size, a
  recovery tag or an enumeration value is derived by the generator; code,
  tests and documents refer to the generated name, never a copied number.
- **Append-only.** A fact already in a version group is never moved,
  resized, retyped, renamed or removed; new facts arrive as additions, and a
  member is retired with `deprecated` (CONTRIBUTING.md, "The absolutes").
- **Generated output is never edited.** `generated_source/` and
  `generated_docs/` are rebuilt from the spec at configure; a fix goes into
  the emitter under `generator/emit/`.
- **Little-endian on disk, on every host.** File bytes are read and written
  through `IFE_Bytes.hpp` or the generated handles, never by casting a
  pointer into the file to a multi-byte type.
- **No reserved identifiers in new names.** A new name does not begin with an
  underscore followed by an uppercase letter, contain two underscores in a
  row, or begin with an underscore at global scope. Existing parameters named
  `__name` are renamed only by a change that is about them.

## Modules and Reuse

Modules are organized by domain, not layer. A module's public API is a small
surface; everything else is file-`static`, in an anonymous namespace, or in a
`detail` namespace. Two modules that share no dependency graph share no file
or library target, and nothing links what it does not use (no umbrella
targets).

Repeated code is extracted, to a place set by its dependencies:

| Scenario | Destination |
|---|---|
| Dependency-free, trivially small | a `static` helper |
| Dependency-free, non-trivial | a domain-grouped utility header |
| External dependencies (SIMD, platform APIs) | its own implementation file and build target, so dependencies do not bleed |

Reuse before writing: `Result`, `Buffer`, `Memory` and the rest of the
shared vocabulary live in Iris-Headers; a new type in IFE that does what one
of them does is a finding.

## Cross-Platform Code

Platform branches live side by side in one function body behind `#if`: one
audit point, one signature, no duplication. Separate per-platform files,
chosen by the build system, only when a whole algorithm or module diverges,
not a few intrinsics.

## State

Use the fewest variables possible: compute in place unless the value is
expensive and reused, outlives its scope, or naming it fixes a real
readability problem. Declare at first use. No dead writes: a value set but
never read is deleted; a single-use intermediate is folded inline. No members
or globals that could be locals.

## Resources

- **RAII always.** No raw `new`/`delete`; every handle, file descriptor and
  mapping is owned by an RAII type or a `unique_ptr` with a custom deleter.
  Move over copy. `std::array`/`std::vector` over C arrays; `std::span` and
  `std::string_view` where ownership is not needed.
- **Ownership webs:** an object that depends on another holds a
  `std::shared_ptr` to it, so nothing can be destroyed while something still
  needs it. The web is acyclic: an owner never holds shared ownership of what
  depends on it; it refers back by ID or `std::weak_ptr`. A shared object
  never holds a callback, because a callback may capture a handle that holds
  the shared object. Hot paths pass references and spans, never shared
  ownership.

## Control Flow

- **Early return.** Check preconditions at the top and leave; never wrap a
  whole body in `if (valid)`. Preconditions are stated in a comment at the top
  of the body (non-null, aligned, initialized, …).
- **Linear over branchy.** Lookup tables and state-machine dispatch over
  `if`/`else if` cascades. Nesting at most 3 levels; deeper becomes a helper.
  No `goto`.
- **Never recurse; queue the calls.** No function calls itself, directly or
  through another. A walk over a tree, a retry, a "repeat until settled" is a
  loop over an explicit queue: push the pending call and run the front entry
  until the queue is empty. FIFO (breadth first) by default; a stack only when
  the order demands it. Pending work is then data, not stack depth: deep input
  cannot overflow the stack, and a cycle shows as a queue that never drains,
  which a bound turns into a loud error. Within a thread, a plain container
  serves (`std::deque`); across threads, Iris-Headers' lock-free queue, whose
  entries are claimed by compare-and-swap so that nothing on it waits.
- **Nothing on a hot path waits.** No lock, sleep or blocking acquire on the
  path of a push, a pop or an issued task; shared work is claimed by
  compare-and-swap, and a thread that loses the claim moves on.
- **No side effects in conditions.** `x = foo(); if (x)`, never
  `if (x = foo())`.

## Errors

Runtime-hardened code uses both mechanisms, each at its own level: exceptions
to capture every kind of failure safely inside a module, `Result` to carry the
debugging detail across its boundary.

- **Inside a module, throw.** Anything the local code cannot handle is thrown
  at the point of failure. RAII unwinds it cleanly.
- **At every API boundary, return a `Result`.** A boundary is a function a
  consumer calls and a thread's entry. No exception crosses one: the boundary
  catches by type and translates into a `Result` carrying
  `Iris::Domain::IRIS_FILE_EXTENSION`, an error code, the offending value and
  where it failed. Boundary functions are `noexcept`, so an exception that
  escapes terminates loudly in a test instead of unwinding into foreign code.
- **Every error carries the offending value:** `"unknown encoding: " +
  std::to_string(e)`, never `"bad encoding"`.
- **Catch specific types.** `catch (...)` appears only as the last handler at a
  boundary, where it becomes a `Result` saying an unknown exception was caught
  and where; never silent. Nothing is printed to `std::cout` or `std::cerr` in
  place of returning the failure.
- `noexcept` on moves, swaps, boundary functions, and leaf helpers that cannot
  fail. An `assert` documents an invariant for debug builds; it is never the
  only guard on a path a release build can reach.

## Typing

- Warnings as errors; `concepts` state every template's contract; exhaustive
  `switch` with no `default:` (CLAUDE.md, Warnings); signed and unsigned mixed only with an explicit
  conversion; non-`const` reference parameters only for outputs, so call sites
  show mutation.
- **Define conventions once.** Reused constants and behaviours become enums
  early and are dispatched on, not copied into a family of functions.
- **Named values for named things.** A value with a meaning is referred to by
  a scoped enumeration or a named constant in code and tests, defined in one
  place. A bare number ties nothing at one call site to the same number
  elsewhere, so its uses cannot be searched for, and a change of meaning
  ripples silently through every copy. The format's values live in
  `spec/*.json`, so their names come from the generated code, and the data
  stays the one source. A name becomes its raw number only at the boundary
  that needs it (the file, the wire).
- **Strong indices.** Indices of different things are different types
  (`StrongIndex<Tag>`), so one cannot be passed where another is expected.

## Naming

Files and functions `snake_case`; classes and structs `PascalCase`; private
helpers file-`static` or in a `detail` namespace; constants
`UPPER_SNAKE_CASE` or `kCamelCase`; macros `UPPER_SNAKE_CASE`; template
parameters short (`T`, `Key`), descriptive where public. A data member is
`_leading_underscore` or `m_name`, as the surrounding code does, outside the
names the standard reserves (The Specification and Generated Code, above).

Accessors name their kind. A predicate returning `bool` is `is_*`. A getter is
the bare noun (`size`, `path`), never `get_*`. A setter is `set_*` taking the
value, so mutation is visible at the call site.

**Shared types take the plain name.** A type whose objects are owned by
`std::shared_ptr` is known by the pointer: the plain name is the alias, and
the class it owns is that name with the suffix `_t`, so the shared ownership
is visible at every use and a caller cannot make an unshared one by accident.

```cpp
class Buffer_t;                              // the object
using Buffer = std::shared_ptr<Buffer_t>;    // what everyone holds
```

The `_t` types live in a namespace (`Iris`), outside the global names POSIX
reserves for its own `_t` types.

Work items are named as TASKS.md and MIGRATION.md number them (`CI-2.4`,
`VL-1.1`, `RC-10`), and a branch is `<kind>/<WorkPackage>_<n>` after one of
them.

## Writing

Prose (comments, documents, commit messages, pull request descriptions) names
the exact mechanism and states why it was chosen.

- **The precise term.** Use the technical name of what was built ("scoped
  enumeration", "FIFO queue", "RAII handle", "content hash") instead of a
  loose stand-in ("by name", "a list", "cleaned up", "a fingerprint") that
  fits several implementations.
- **The reason.** State the risk removed or the requirement met. A change
  described without its reason cannot be checked against it.
- **No contrast pairs for emphasis.** A claim restated as its own negation
  ("X, never Y", "X, not Y") adds rhythm and no information. State the
  positive claim once; mention a replaced approach only to say what changed,
  plainly ("instead of by number").

## Comments and Documentation

Block comments state the intent wherever the *why* is not visible in the code:
the write head's claim protocol, the tile frame's backward layout, byte order
and recovery tags. A comment that restates the code is replaced by clearer code.
Dead code (commented-out blocks, unreachable branches, orphaned functions,
declarations with no definition) is deleted; git keeps the history.

Documents describing more than one type or component carry **Mermaid
diagrams**, reviewed like code, kept true to the source, and placed beside the
prose they illustrate. Every diagram must render.

- `classDiagram` for any set of collaborating types: key members and the
  relationships (`<|--` inheritance, `..|>` implementation, `*--`
  composition, `-->` association).
- `flowchart` for dependencies and control or data flow; `sequenceDiagram`
  for multi-actor lifecycles.

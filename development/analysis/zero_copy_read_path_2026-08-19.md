# The zero-copy span read path (ZC-1 to ZC-5)

> **Record.** Written 2026-08-19 in MIGRATION.md at the repository root; moved here on
> 2026-11-24 when `development/` replaced the root planning documents (git history
> keeps the originals; the last commit that holds them is 005347e). Kept as written,
> with local paths, sibling-checkout paths and a quotation restated for a public
> repository. Open items are tracked in [WBS.md](../WBS.md) under the same IDs; their
> boxes here read `(open: WBS.md)`. Dated records keep the names they had (Iris-Codec
> is now Iris-Slide).


FastFHIR completed this exact migration on 2026-08-19 (TASKS.md OPEN TOPIC §C);
IFE serialization must adopt the same structure so the read path stays
allocation-free and the "0 heap allocations / nanosecond reads" claim holds.
**This is the reference implementation: `FastFHIR::reflected_fields_view()`
returning `std::span<const FF_FieldInfo>` over a static table, and
`Node::fields()` returning that span.** The old by-value
`reflected_fields()` → `std::vector<FF_FieldInfo>` was deleted — every caller
migrated, nothing needs an owning copy on the read path.

### The rule

**Read-path metadata and entry access is a const lens over resident memory:
`std::span<const T>` over static (generated) or in-place (block) storage.
Never materialize a `std::vector` per call on the read path.** Builder-side
vectors that own data being *written* are fine — the migration is about reads.

### What this looks like in IFE

| Current IFE pattern | Must become | Notes |
|---|---|---|
| `generated_source/IFE_Blocks.hpp` — per-block static layout tables | `std::span<const T>` accessors over the static arrays (the FastFHIR `reflected_fields_view` shape) | tables are already static/constexpr; expose spans, never copy |
| `include/IrisFileExtension.hpp` — `Layer = std::vector<TileEntry>`, `Layers = std::vector<Layer>`, `AttributeSet = std::vector<AttributeNode>` | lazy spans over the resident `Window` bytes (resident mode), owning vectors only where data must outlive the window | the `Window::__pages` cache mode legitimately owns; span-ify the resident path |
| `src/IFE_Runtime.cpp` — per-call `std::vector<AttributeSlice> slices` | parse into spans over the block, or reuse one buffer — no per-call allocation | measured cost of the vector pattern in FastFHIR: ~65% of `entries()`'s time was per-element `push_back` machinery at `-O0`; at `-O3` it is still a real per-call allocation on a path documented allocation-free |
| `const std::vector<T>& entries` in the generated `*CreateInfo` structs | keep (write side, owns data) — but read it back through spans | `store()` reads `vector::size()`; that is the builder, not the read path |

### Why (numbers, FastFHIR 2026-08-19, 50.8 MiB bundle, min of 7)

- `Bundle.entry.entries()`: 856 µs/call → **36 µs** at `-O3` after removing the
  allocating reflection path (and the reflective walk: 0.75 → **0.60 µs/entry**
  once `Node::fields()` stopped allocating per block).
- `node_size()` was building an entire `std::vector` just to call `.size()` —
  `FIELD_COUNT` is a compile-time constant. Now 17.5 ns/call.
- The Debug-build trap applies here too: all these costs are ~10× higher at
  `-O0` (FastFHIR's `ninja` preset is Debug). **Measure IFE reads against a
  Release build** (`-DCMAKE_BUILD_TYPE=Release`) or every span decision will be
  made on inflated numbers.

### Checklist for IFE code

- (open: WBS.md) Block layout tables in `generated_source/IFE_Blocks.hpp` exposed as
      `std::span<const T>` (generator change in `generator/emit/cpp.py`), never
      returned by value.
- (open: WBS.md) Read path (`IFE_Runtime.cpp` / `IFE_Window.cpp`): no `std::vector`
      constructed per call when the source data is resident-addressable.
- (open: WBS.md) `AttributeSlice` / tile-entry access goes through spans over the block,
      not materialized `std::vector` layers.
- (open: WBS.md) Wire-format gate (if any) and the corpus harness measure against a
      Release build.
- (open: WBS.md) Benchmark parity: any public API change (e.g. a function now returning
      `std::span`) is noted for the companion benchmark repo.

Reference commit context: FastFHIR `generator/emit/views.py` now emits only
`reflected_fields_view` (span); `Node::fields()`, `ObjectHandle::fields()`,
`MutableEntry::fields()` all return `std::span<const FF_FieldInfo>`; the
allocating variants were deleted from the emitter. Copy that structure.

---

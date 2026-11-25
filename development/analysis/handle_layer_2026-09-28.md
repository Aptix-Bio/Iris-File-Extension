# The handle layer: Builder, Parser and the namespace hierarchy

> **Record.** Written 2026-09-28 in MIGRATION.md at the repository root; moved here on
> 2026-11-24 when `development/` replaced the root planning documents (git history
> keeps the originals; the last commit that holds them is 005347e). Kept as written,
> with local paths, sibling-checkout paths and a quotation restated for a public
> repository. Open items are tracked in [WBS.md](../WBS.md) under the same IDs; their
> boxes here read `(open: WBS.md)`. Dated records keep the names they had (Iris-Codec
> is now Iris-Slide).


Decided 2026-09-28. The executable work orders lived in a local hand-off note,
never committed; what must outlive it is here.

- **Layering.** An application *owns* an IFE handle: Iris-Slide's Encoder holds an
  `Iris::File::Builder`, its Slide holds an `Iris::File::Parser`. The Builder
  claims space and writes every block; the Encoder does sources, codecs and
  threads. The Parser owns the read-only mapping and bounds-checked bytes; the
  Slide decodes. Worked example of the split: the *Encoder* names the temp file,
  hands it to the Builder, and moves the finished file — the Builder only
  writes where it is told and leaves a closed file at `finalize`.
- **IFE is independent of Iris-Slide.** The codec depends on IFE, never the
  reverse. IFE's handles are built first, anticipating the codec; the codec is
  rewritten onto them later.
- **Arenas never remap.** A HUGE sparse reservation (Builder default 8 GiB);
  growth is pages touched inside it; exhausting it is terminal.
- **NULL_TILE is a legitimate "no tile here."** A Builder refuses to finalize a
  tile nobody accounted for; it never publishes one as NULL_TILE.
- **Tile frames:** optional (and encouraged) on single-plane layers; mandatory
  on Z-stacked layers — to be written into the **1.1 draft** (no version bump).
- **Namespaces:** IFE is `Iris::File::` (renamed from `IFE::` 2026-09-28, before
  the `IFE::` spelling was ever pushed). Core stays `Iris::` (`Iris::Memory`).
  `Iris::Codec::` for the codec is wanted later, but only after the format
  vocabulary Iris-Headers still declares in `namespace IrisSlide` (`Metadata`,
  `Encoding`, `AssociatedImageInfo`, …) moves to `Iris::`/`Iris::File::` —
  renamed in place it would make File depend on Codec. The `IFE_` prefixes
  (`IFE_EXPORT`, the `IFE_*` global aliases, `IFE_*.hpp`) name the product and
  stay. `Iris::File` no longer re-exports `Iris::` through a using-directive: a
  consumer that wants both unqualified writes both.
- **The layout is the caller's (2026-09-30).** A Builder controls how the
  stream is mutated — claim space at the head, fill it — and never where a
  block goes, as in FastFHIR, where the layout belongs to the caller of the
  Builder's API (decided 2026-09-30). The
  first `finalize` wrote the whole structure in its own order and sorted the
  attributes, which put METADATA after the mutable attribute blocks and image
  bytes ahead of the tile table; Iris-Codec (now Iris-Slide) could not reproduce the layout it
  has always written (fixed-size blocks ahead of ones an edit replaces:
  tiles → tile table → METADATA → ICC → images → attributes). `finalize` now
  only seals (`BuilderFinalizeInfo{tileTable, metadata, revision}`); the caller
  composes every structure block through `claim` / `fill` / `append` behind
  `->`, each landing at the head when called, from what the handle reports it
  placed — `tile_offsets()` (refusing a tile nobody accounted for) and
  `image_entries()`; `query()` reads the stream back. (Briefly the handle
  placed the tile table, images array and attributes itself —
  `append_tile_table` / `append_images` / `append_attributes`; folded into this
  shape the same day, 4abcf16.) Nothing is sorted: attribute entries go down in
  the order the caller writes them. `ife_builder_layered_tests` (h) writes the same inputs in
  two layouts; the shared sweep fixture is byte-identical to before.

Implemented 2026-09-28: the rename; the Builder's application tier
(`set_tile_table` / `append_tile` / `append_null_tile` / `append_image` /
`finalize`, block tier behind `->`) with the header region reserved and a closed
file at finalize; `Parser::open` owning its mapping with zero-copy
`tile()` / `image()` and `tile_planes()`; tile entries bounds-checked on read;
Z-stacked frames required (1.1 draft) and validated; generated `store()`
refusing values wider than their packed fields; recovery enumerators bounded by
geometry (an ASan overflow); `Iris::Memory` reserve-not-commit on anonymous
arenas (Iris-Headers). Tests: `ife_builder_tests`, `ife_builder_layered_tests`,
`ife_parser_tests`, `ife_blocks_tests`, and `ife_readme_compiles` (the README's
examples, compiled as published).

Consumed 2026-09-30 (Iris-Codec, now Iris-Slide, 8a0f93f, landed with this repository's 4abcf16):
the Encoder writes through a Builder — `append_tile` / `append_null_tile` from
its pool, then composes its own layout from `tile_offsets()` / `image_entries()`
(tile table, METADATA claimed early and filled last, ICC, images, attributes)
and `finalize` — and moves the scratch file into place; the Slide holds a `Parser`. The codec's file-mapping layer
(`IrisCodecFile.*`) is deleted. Its encoder output is byte-identical to its
pre-Builder output. It writes no tile frames on single-plane layers; an Iris
source's Z-stacked layers pass through framed, with their plane counts.

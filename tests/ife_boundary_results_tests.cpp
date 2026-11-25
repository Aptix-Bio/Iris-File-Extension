/**
 * @file ife_boundary_results_tests.cpp
 * @brief No exception crosses IFE's public read API (R-8, development/WBS.md):
 *        every read entry point is `noexcept`, returns an `Iris::Result`, and
 *        writes the value it produces through an output reference
 *        (development/style_guide.md, Errors).
 *
 * What this pins:
 *   1. The signature and `noexcept` of every converted function, at compile
 *      time, so a later change that lets one throw or return its value again
 *      fails the build here.
 *   2. Each failure the read path meets comes back as a failure Result under
 *      `Domain::IRIS_FILE_EXTENSION` with its `FileError` and the offending
 *      value in its message: a damaged block, a TILE_TABLE.MIP_LEVELS the tile
 *      cannot hold, a tile the slide does not have, a label no image carries,
 *      a path that cannot be mapped, bytes too few to hold a header.
 *   3. The output is written only on success: a failed call leaves it as the
 *      caller had it.
 *   4. A Parser whose lift failed fails again on the next call, instead of
 *      handing back a half-lifted structure.
 *
 * Self-contained (no framework, no corpus): the slides are Builder-written.
 * Non-zero exit on failure.
 */
#include "IFE_Builder.hpp"
#include "IFE_Parser.hpp"
#include "IFE_Primitives.hpp"
#include "IFE_Recovery.hpp"
#include "IrisFileExtension.hpp"

#include "ife_builder_fixture.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Iris;
using namespace Iris::File;

namespace b = ::Iris::File::blocks;
namespace k = ::Iris::File::constants;

using Span = std::span<const BYTE>;

// ---- 1. every converted function: its signature and noexcept ------------- //

// Declared and never defined: they appear only in unevaluated operands, and
// have external linkage so the compiler does not report them unused.
namespace probe {

template <class T> T& out() noexcept;   // an lvalue of T

const Parser&                parser() noexcept;
const Parser_t&              body() noexcept;
const Recovery&              recovery() noexcept;
const FileAccessInfo&        access_info() noexcept;
const std::filesystem::path& a_path() noexcept;

// The free functions.
static_assert(std::is_same_v<decltype(abstract_file_structure(access_info(), out<Abstraction::File>())), Result>);
static_assert(noexcept(abstract_file_structure(access_info(), out<Abstraction::File>())));
static_assert(std::is_same_v<decltype(generate_file_map(access_info(), out<Abstraction::FileMap>())), Result>);
static_assert(noexcept(generate_file_map(access_info(), out<Abstraction::FileMap>())));
// The two that already complied, held to it.
static_assert(noexcept(is_iris_slide_file(access_info())));
static_assert(noexcept(validate_file_structure(access_info())));

// Parser::open writes the handle it makes.
static_assert(std::is_same_v<decltype(Parser::open(a_path(), out<Parser>())), Result>);
static_assert(noexcept(Parser::open(a_path(), out<Parser>())));

// The Parser handle.
static_assert(std::is_same_v<decltype(parser().abstract_file_structure(out<Abstraction::File>())), Result>);
static_assert(noexcept(parser().abstract_file_structure(out<Abstraction::File>())));
static_assert(std::is_same_v<decltype(parser().abstraction(out<const Abstraction::File*>())), Result>);
static_assert(noexcept(parser().abstraction(out<const Abstraction::File*>())));
static_assert(std::is_same_v<decltype(parser().tile(0u, 0u, out<Span>())), Result>);
static_assert(noexcept(parser().tile(0u, 0u, out<Span>())));
static_assert(std::is_same_v<decltype(parser().tile_planes(0u, 0u, out<std::uint16_t>())), Result>);
static_assert(noexcept(parser().tile_planes(0u, 0u, out<std::uint16_t>())));
static_assert(std::is_same_v<decltype(parser().image(std::string{}, out<Span>())), Result>);
static_assert(noexcept(parser().image(std::string{}, out<Span>())));

// The body behind it, which also carries the file map.
static_assert(std::is_same_v<decltype(body().abstract_file_structure(out<Abstraction::File>())), Result>);
static_assert(noexcept(body().abstract_file_structure(out<Abstraction::File>())));
static_assert(std::is_same_v<decltype(body().abstraction(out<const Abstraction::File*>())), Result>);
static_assert(noexcept(body().abstraction(out<const Abstraction::File*>())));
static_assert(std::is_same_v<decltype(body().tile(0u, 0u, out<Span>())), Result>);
static_assert(noexcept(body().tile(0u, 0u, out<Span>())));
static_assert(std::is_same_v<decltype(body().tile_planes(0u, 0u, out<std::uint16_t>())), Result>);
static_assert(noexcept(body().tile_planes(0u, 0u, out<std::uint16_t>())));
static_assert(std::is_same_v<decltype(body().image(std::string{}, out<Span>())), Result>);
static_assert(noexcept(body().image(std::string{}, out<Span>())));
static_assert(std::is_same_v<decltype(body().generate_file_map(out<Abstraction::FileMap>())), Result>);
static_assert(noexcept(body().generate_file_map(out<Abstraction::FileMap>())));

// The recovery census.
static_assert(std::is_same_v<decltype(recovery().scan(out<Abstraction::FileMap>())), Result>);
static_assert(noexcept(recovery().scan(out<Abstraction::FileMap>())));
static_assert(std::is_same_v<decltype(recovery().find_gaps(out<Abstraction::FileMap>())), Result>);
static_assert(noexcept(recovery().find_gaps(out<Abstraction::FileMap>())));
static_assert(std::is_same_v<decltype(recovery().census(out<Recovery::Census>())), Result>);
static_assert(noexcept(recovery().census(out<Recovery::Census>())));

}  // namespace probe

namespace {

// ---- the harness ---------------------------------------------------------- //

int g_failures = 0;

#define IFE_CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        ++g_failures; \
    } \
} while (0)

/// A failure under IFE's domain, with @p code, whose message names @p value.
bool fails_with(const Result& result, FileError code, const std::string& value) {
    const bool named = result.message.find(value) != std::string::npos;
    if (!named)
        std::fprintf(stderr, "  message does not name '%s': %s\n", value.c_str(), result.message.c_str());
    return result.failed() && result.domain == Domain::IRIS_FILE_EXTENSION &&
           result.code_as<FileError>() == code && named;
}

/// A one-tile UASTC slide whose TILE_TABLE declares `levels` mip levels.
std::vector<BYTE> uastc_slide(std::uint16_t levels) {
    BuilderTileTableInfo grid;
    grid.encoding      = static_cast<Encoding>(k::TileEncodings::TILE_ENCODING_UASTC);
    grid.format        = FORMAT_R8G8B8A8;
    grid.extent.width  = 256;
    grid.extent.height = 256;
    grid.extent.layers = {LayerExtent{.xTiles = 1, .yTiles = 1, .scale = 1.0f}};
    const Builder builder = Builder::create({.capacity = Size{1} << 20});
    builder.set_tile_table(grid);
    const std::vector<BYTE> stream(32, 0xCD);   // contents are not IFE's business
    builder.append_tile(0, 0, stream.data(), stream.size());
    const Offset tiles_at   = builder->append(b::TileOffsetsCreateInfo{.entries = builder.tile_offsets()});
    const Offset extents_at = builder->append(b::LayerExtentsCreateInfo{
        .entries = {{.X_TILES = 1, .Y_TILES = 1, .SCALE = 1.0f, .Z_PLANES = 0}}});
    const Offset table_at = builder->append(b::TileTableCreateInfo{
        .ENCODING             = k::TileEncodings::TILE_ENCODING_UASTC,
        .FORMAT               = static_cast<k::PixelFormats>(grid.format),
        .TILE_OFFSETS_OFFSET  = tiles_at,
        .LAYER_EXTENTS_OFFSET = extents_at,
        .X_EXTENT             = grid.extent.width,
        .Y_EXTENT             = grid.extent.height,
        .TILE_LENGTH          = 256,
        .MIP_LEVELS           = levels});
    const Offset metadata_at = builder->append(b::MetadataCreateInfo{.MICRONS_PIXEL = 0.25f});
    builder.finalize({.tileTable = table_at, .metadata = metadata_at});
    return {builder->base(), builder->base() + builder.head()};
}

}  // namespace

int main() {
    const std::vector<BYTE> slide = ife_builder_fixture();
    const FileAccessInfo    clean{slide.data(), slide.size()};

    // ---- a clean slide reads through every converted function -------------- //
    {
        Abstraction::File file;
        IFE_CHECK(abstract_file_structure(clean, file).succeeded());
        IFE_CHECK(file.tileTable.layers.size() == 2);

        Abstraction::FileMap map;
        IFE_CHECK(generate_file_map(clean, map).succeeded());
        IFE_CHECK(!map.empty());

        const Parser reader(clean);
        const Abstraction::File* lifted = nullptr;
        IFE_CHECK(reader.abstraction(lifted).succeeded());
        IFE_CHECK(lifted != nullptr);
        const Abstraction::File* again = nullptr;
        IFE_CHECK(reader.abstraction(again).succeeded());
        IFE_CHECK(again == lifted);   // lifted once, and the same object handed out

        Span stream;
        IFE_CHECK(reader.tile(0, 0, stream).succeeded());
        IFE_CHECK(!stream.empty() && stream.data() >= clean.file_ptr &&
                  stream.data() + stream.size() <= clean.file_ptr + clean.file_size);
        IFE_CHECK(reader.tile(1, 3, stream).succeeded());
        IFE_CHECK(stream.empty());    // NULL_TILE: no tile there, an expected outcome

        std::uint16_t planes = 99;
        IFE_CHECK(reader.tile_planes(1, 0, planes).succeeded());
        IFE_CHECK(planes == 3);
        IFE_CHECK(reader.tile_planes(1, 3, planes).succeeded());
        IFE_CHECK(planes == 0);

        IFE_CHECK(reader.image("label", stream).succeeded());
        IFE_CHECK(stream.size() == 24);

        Abstraction::FileMap body_map;
        IFE_CHECK(reader->generate_file_map(body_map).succeeded());
        IFE_CHECK(body_map.size() == map.size());

        const Recovery census_of(clean);
        Abstraction::FileMap scanned;
        IFE_CHECK(census_of.scan(scanned).succeeded());
        IFE_CHECK(!scanned.empty());
        IFE_CHECK(census_of.find_gaps(scanned).succeeded());
        Recovery::Census census;
        IFE_CHECK(census_of.census(census).succeeded());
        IFE_CHECK(census.islands.size() == 1 && census.points.empty());
    }

    // ---- a missing tile: the index asked for, in the message ---------------- //
    {
        const Parser reader(clean);
        const Span   before{slide.data(), 1};
        Span         stream = before;
        IFE_CHECK(fails_with(reader.tile(0, 5, stream), FileError::NO_SUCH_TILE, "tile 5"));
        IFE_CHECK(stream.data() == before.data() && stream.size() == before.size());   // untouched
        IFE_CHECK(fails_with(reader.tile(2, 0, stream), FileError::NO_SUCH_TILE, "layer 2"));
        std::uint16_t planes = 99;
        IFE_CHECK(fails_with(reader.tile_planes(1, 4, planes), FileError::NO_SUCH_TILE, "tile 4"));
        IFE_CHECK(planes == 99);
    }

    // ---- a missing label: the label asked for, in the message -------------- //
    {
        const Parser reader(clean);
        Span stream;
        IFE_CHECK(fails_with(reader.image("no such label", stream), FileError::NO_SUCH_IMAGE,
                             "'no such label'"));
        IFE_CHECK(stream.empty());
    }

    // ---- a damaged file: the block, and the value it stores ---------------- //
    {
        std::vector<BYTE> damaged = slide;
        const Offset table = versioned_root(damaged.data(), damaged.size()).tile_table_offset().__offset;
        constexpr BYTE FLIP = 0xFF;   // every bit of VALIDATION's low byte (little-endian)
        damaged[table] ^= FLIP;       // the TILE_TABLE's VALIDATION no longer names its own offset
        const FileAccessInfo at{damaged.data(), damaged.size()};
        // The block, the byte, and the value VALIDATION now stores.
        const std::string    where = "TILE_TABLE.VALIDATION at byte " + std::to_string(table) +
                                     " stores " + std::to_string(table ^ FLIP);

        Abstraction::File file;
        file.header.revision = 7;
        IFE_CHECK(fails_with(abstract_file_structure(at, file), FileError::BAD_VALIDATION, where));
        IFE_CHECK(file.header.revision == 7);   // untouched

        const Parser reader(at);
        const Abstraction::File* lifted = nullptr;
        IFE_CHECK(fails_with(reader.abstraction(lifted), FileError::BAD_VALIDATION, where));
        IFE_CHECK(lifted == nullptr);
        // The lift is not cached as done: the next call refuses the same way.
        IFE_CHECK(fails_with(reader.abstraction(lifted), FileError::BAD_VALIDATION, where));
        Span stream;
        IFE_CHECK(fails_with(reader.tile(0, 0, stream), FileError::BAD_VALIDATION, where));
        IFE_CHECK(fails_with(reader.image("label", stream), FileError::BAD_VALIDATION, where));
    }

    // ---- a MIP_LEVELS the tile cannot hold: CONFORMANCE, with the count ----- //
    {
        const std::vector<BYTE> eight = uastc_slide(8);   // 256 px holds 7 levels, down to 4 px
        const FileAccessInfo    at{eight.data(), eight.size()};
        IFE_CHECK(fails_with(validate_file_structure(at), FileError::CONFORMANCE, "MIP_LEVELS 8"));
        Abstraction::File file;
        IFE_CHECK(fails_with(abstract_file_structure(at, file), FileError::CONFORMANCE, "MIP_LEVELS 8"));
        const Abstraction::File* lifted = nullptr;
        IFE_CHECK(fails_with(Parser(at).abstraction(lifted), FileError::CONFORMANCE, "MIP_LEVELS 8"));

        const std::vector<BYTE> seven = uastc_slide(7);
        IFE_CHECK(abstract_file_structure({seven.data(), seven.size()}, file).succeeded());
        IFE_CHECK(file.tileTable.mipLevels == 7);
    }

    // ---- a path that cannot be mapped: the path, in the message ------------ //
    {
        const std::filesystem::path missing =
            std::filesystem::temp_directory_path() / "ife_boundary_results_no_such_slide.iris";
        std::filesystem::remove(missing);
        Parser opened;
        IFE_CHECK(fails_with(Parser::open(missing, opened), FileError::NOT_MAPPED, missing.string()));
        IFE_CHECK(!opened);   // untouched
    }

    // ---- bytes too few to hold a header ------------------------------------- //
    {
        const std::vector<BYTE> four(4, 0);
        const FileAccessInfo    at{four.data(), four.size()};
        Abstraction::FileMap map;
        IFE_CHECK(fails_with(generate_file_map(at, map), FileError::NOT_CONSTRUCTED, "FILE_HEADER at byte 0"));
        IFE_CHECK(map.empty());
        Abstraction::File file;
        IFE_CHECK(fails_with(abstract_file_structure(at, file), FileError::NOT_CONSTRUCTED,
                             "FILE_HEADER at byte 0"));
    }

    if (g_failures == 0) std::printf("ife_boundary_results_tests: PASS\n");
    return g_failures == 0 ? 0 : 1;
}

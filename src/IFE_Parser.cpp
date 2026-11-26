/**
 * @file IFE_Parser.cpp
 * @brief Implementation of the read handle/body declared in "IFE_Parser.hpp"
 *        and "IrisFileExtension.hpp".
 * @copyright Iris Developers, 2025-2026
 */

#include "IFE_Parser.hpp"
#include "IFE_Failure.hpp"      // Failure, boundary
#include "IFE_Primitives.hpp"   // tile_frame_error
#include "IrisMemory.hpp"       // priv: Iris::Memory, for Parser::open

#include <algorithm>
#include <string>

namespace Iris::File {

namespace k = ::Iris::File::constants;
namespace b = ::Iris::File::blocks;

namespace {

/// The lifted structure, or the lift's failure thrown on to the caller's
/// boundary with its code and place.
const Abstraction::File& lifted(const Parser_t& parser) {
    const Abstraction::File* file = nullptr;
    if (const Result result = parser.abstraction(file); result.failed()) throw Failure(result);
    return *file;
}

/// The entry for (layer, tile), or NO_SUCH_TILE naming what was asked for.
const Abstraction::TileEntry& entry_at(const Abstraction::File& file, uint32_t layer, uint32_t tile) {
    const auto& layers = file.tileTable.layers;
    if (layer >= layers.size())
        throw Failure(FileError::NO_SUCH_TILE, "IFE Parser: layer " + std::to_string(layer) +
                      " of a slide with " + std::to_string(layers.size()) + " layers");
    if (tile >= layers[layer].size())
        throw Failure(FileError::NO_SUCH_TILE, "IFE Parser: tile " + std::to_string(tile) + " of layer " +
                      std::to_string(layer) + ", which has " +
                      std::to_string(layers[layer].size()) + " tiles");
    return layers[layer][tile];
}

/// A span over [offset, offset + size), or OUT_OF_BOUNDS if it leaves the
/// file. The abstraction has already checked every range it hands out; this
/// is the last line, so a span can never point past the mapping.
std::span<const BYTE> span_at(const FileAccessInfo& info, Offset offset, Size size, const std::string& what) {
    // Never `offset + size <= file_size`: that sum can wrap.
    if (offset > info.file_size || info.file_size - offset < size)
        throw Failure(FileError::OUT_OF_BOUNDS, "IFE Parser: " + what + " (OFFSET " + std::to_string(offset) +
                      ", SIZE " + std::to_string(size) + ") lies outside the " +
                      std::to_string(info.file_size) + "-byte file");
    return {info.file_ptr + offset, static_cast<std::size_t>(size)};
}

}  // namespace

// =====================================================================
// Parser_t — the body
// =====================================================================

Result Parser_t::abstraction(const Abstraction::File*& file) const noexcept
{
    return boundary("Parser::abstraction", [&] {
        // A throw leaves the flag unset, so a damaged file refuses on every
        // call rather than handing back a half-lifted structure.
        std::call_once(m_lifted, [this] {
            Abstraction::File lift;
            if (const Result result = ::Iris::File::abstract_file_structure(m_info, lift); result.failed())
                throw Failure(result);
            m_abstraction = std::move(lift);
        });
        file = &m_abstraction;
    });
}

Result Parser_t::tile(uint32_t layer, uint32_t tile, std::span<const BYTE>& stream) const noexcept
{
    return boundary("Parser::tile", [&] {
        const Abstraction::TileEntry& entry = entry_at(lifted(*this), layer, tile);
        stream = entry.offset == k::NULL_TILE   // no tile at this grid position
            ? std::span<const BYTE>{}
            : span_at(m_info, entry.offset, entry.size,
                      "layer " + std::to_string(layer) + ", tile " + std::to_string(tile));
    });
}

Result Parser_t::tile_planes(uint32_t layer, uint32_t tile, uint16_t& planes) const noexcept
{
    return boundary("Parser::tile_planes", [&] {
        const Abstraction::File&      file  = lifted(*this);
        const Abstraction::TileEntry& entry = entry_at(file, layer, tile);
        if (entry.offset == k::NULL_TILE) {
            planes = 0;
            return;
        }
        const uint16_t layer_planes = file.tileTable.planes[layer];   // normalised: at least 1
        if (layer_planes <= 1) {
            planes = 1;
            return;
        }

        // Every stream of a Z-stacked layer is framed, and abstraction() has
        // already refused a file where one is not; checked again because this
        // is the read that depends on it.
        const b::FILE_HEADER header = versioned_root(m_info.file_ptr, m_info.file_size);
        // Global indices ascend by layer, row-major within a layer.
        uint64_t global = tile;
        for (uint32_t l = 0; l < layer; ++l) global += file.tileTable.layers[l].size();
        if (const char* why = tile_frame_error(m_info.file_ptr, m_info.file_size, header.__version,
                                               entry.offset, global, layer_planes))
            throw Failure(FileError::BAD_TILE_ENTRY, "IFE Parser: layer " + std::to_string(layer) +
                          ", tile " + std::to_string(tile) + " " + why);
        const b::TILE_PIXEL_DATA frame{m_info.file_ptr, entry.offset, m_info.file_size, header.__version};
        planes = std::max<uint16_t>(frame.z_planes().value_or(0), 1);   // zero means one plane
    });
}

Result Parser_t::image(const std::string& label, std::span<const BYTE>& stream) const noexcept
{
    return boundary("Parser::image", [&] {
        const Abstraction::File& file  = lifted(*this);
        const auto               found = file.images.find(label);
        if (found == file.images.end())
            throw Failure(FileError::NO_SUCH_IMAGE, "IFE Parser: no associated image labelled '" + label + "'");
        stream = span_at(m_info, found->second.offset, found->second.byteSize,
                         "associated image '" + label + "'");
    });
}

// =====================================================================
// Parser — the handle
// =====================================================================

Parser::Parser(const FileAccessInfo& info)
    : Base(std::make_shared<Parser_t>(info)) {}

Result Parser::open(const std::filesystem::path& path, Parser& parser) noexcept
{
    return boundary("Parser::open", [&] {
        auto memory = std::make_shared<Iris::Memory>();
        const Iris::Result made = Iris::create_memory(
            Iris::MemoryCreateInfo{.filepath = path, .read_only = true}, *memory);
        if (made.failed())
            throw Failure(FileError::NOT_MAPPED, "IFE Parser: could not open " + path.string() + " (" +
                          made.message + ")");
        // A read-only arena's capacity is the file's own size.
        const FileAccessInfo info{memory->base(), memory->capacity()};
        parser = Parser(std::make_shared<Parser_t>(info, std::shared_ptr<const void>(std::move(memory))));
    });
}

Result Parser::is_iris_slide_file() const noexcept { return get()->is_iris_slide_file(); }
Result Parser::validate_file_structure() const noexcept { return get()->validate_file_structure(); }
Result Parser::abstract_file_structure(Abstraction::File& file) const noexcept {
    return get()->abstract_file_structure(file);
}
Result Parser::abstraction(const Abstraction::File*& file) const noexcept { return get()->abstraction(file); }
Result Parser::tile(uint32_t layer, uint32_t tile, std::span<const BYTE>& stream) const noexcept {
    return get()->tile(layer, tile, stream);
}
Result Parser::tile_planes(uint32_t layer, uint32_t tile, uint16_t& planes) const noexcept {
    return get()->tile_planes(layer, tile, planes);
}
Result Parser::image(const std::string& label, std::span<const BYTE>& stream) const noexcept {
    return get()->image(label, stream);
}
const FileAccessInfo& Parser::info() const noexcept { return get()->info(); }
const BYTE*           Parser::data() const noexcept { return get()->data(); }
Size                  Parser::size() const noexcept { return get()->size(); }

}  // namespace Iris::File

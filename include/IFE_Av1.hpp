/**
 * @file IFE_Av1.hpp
 * @brief The reader behind the AV1 external checks: it walks a
 *        TILE_ENCODING_AV1 tile byte-stream to its first sequence header and
 *        takes the syntax elements spec/ife_fields.json's external_check
 *        compares with IFE's blocks (D31, development/ARCHITECTURE.md).
 * @copyright Iris Developers, 2025-2026
 *
 * IFE does not validate the AV1 stream. This reader finds the elements the
 * generated `check_av1_stream` compares and reads nothing past the last of
 * them (`color_range`): no frame header, no tile data, no rule of the stream's
 * own. Every element is read as the AV1 specification's syntax tables write it
 * (https://aomediacodec.github.io/av1-spec/, read 2026-11-26): section 5.3
 * (OBU header), 4.10.5 (leb128), 4.10.3 (uvlc), 5.5.1 (sequence_header_obu),
 * 5.5.2 (color_config); an element the syntax infers takes the inferred value.
 *
 * Every read is bounded by the bytes it was given: an OBU by the stream, an
 * element by its OBU. A stream it cannot walk is a CONFORMANCE failure on
 * TILE_PIXEL_DATA naming the element it could not read. noexcept and
 * allocation-free, returning a Status as the generated validators do, so the
 * opt-in validation layer can call it.
 *
 * Private: in namespace `detail`, included by src/IFE_Runtime.cpp and the
 * generated IFE_Validation.cpp, so the library and the layer run one reader.
 * CMake does not install it (IFE_SourcesExport leaves it out); Bazel declares
 * it in :src_headers, since its sandbox hides an undeclared header.
 */
#ifndef IFE_Av1_hpp
#define IFE_Av1_hpp

#include <cstdint>

#include "IFE_Blocks.hpp"   // Status, ValidationHooks, the generated Av1StreamFields

namespace Iris::File::detail {

/**
 * @brief Read the syntax elements of `blocks::Av1StreamFields` from a
 *        TILE_ENCODING_AV1 tile byte-stream's first sequence header.
 *
 * OBUs before it are stepped over by their obu_size, whatever their type.
 * Success leaves @p fields holding every element; a failure is CONFORMANCE on
 * TILE_PIXEL_DATA, `field` naming what could not be read and `found` the
 * value or length met there, and writes the reason to @p hooks' diagnostic.
 *
 * Preconditions: @p stream addresses @p size readable bytes (it may be null
 * only when @p size is 0); @p hooks may be null.
 */
[[nodiscard]] inline blocks::Status read_av1_stream(const BYTE* stream, Size size,
                                                    blocks::Av1StreamFields& fields,
                                                    const blocks::ValidationHooks* hooks) noexcept;

// ============================================================================
// Implementation
// ============================================================================

namespace av1 {

// The OBU header byte (section 5.3.2): obu_forbidden_bit, obu_type (4 bits),
// obu_extension_flag, obu_has_size_field, obu_reserved_1bit.
inline constexpr unsigned FORBIDDEN_BIT       = 0x80;
inline constexpr unsigned TYPE_SHIFT          = 3;
inline constexpr unsigned TYPE_MASK           = 0x0F;
inline constexpr unsigned EXTENSION_FLAG      = 0x04;
inline constexpr unsigned HAS_SIZE_FIELD      = 0x02;
inline constexpr unsigned OBU_SEQUENCE_HEADER = 1;   // obu_type (section 6.2.2)

// leb128() (section 4.10.5): at most 8 bytes, a value at most 2^32 - 1.
inline constexpr unsigned      LEB128_MAX_BYTES = 8;
inline constexpr unsigned      LEB128_MORE      = 0x80;
inline constexpr unsigned      LEB128_PAYLOAD   = 0x7F;
inline constexpr std::uint64_t LEB128_MAX       = 0xFFFFFFFFu;

// sequence_header_obu() and color_config() (sections 5.5.1, 5.5.2, 6.4.2).
inline constexpr std::uint64_t LAST_LEVEL_WITHOUT_TIER     = 7;   // seq_tier is coded above it
inline constexpr std::uint64_t SELECT_SCREEN_CONTENT_TOOLS = 2;
inline constexpr std::uint64_t PROFILE_444                 = 1;   // mono_chrome is not coded
inline constexpr std::uint64_t PROFILE_PROFESSIONAL        = 2;   // twelve_bit may be coded
inline constexpr std::uint64_t CP_BT_709                   = 1;
inline constexpr std::uint64_t CP_UNSPECIFIED              = 2;
inline constexpr std::uint64_t TC_UNSPECIFIED              = 2;
inline constexpr std::uint64_t TC_SRGB                     = 13;
inline constexpr std::uint64_t MC_IDENTITY                 = 0;
inline constexpr std::uint64_t MC_UNSPECIFIED              = 2;
inline constexpr std::uint64_t FULL_SWING                  = 1;

/// A stream the walk cannot read: CONFORMANCE on TILE_PIXEL_DATA.
[[nodiscard]] inline blocks::Status unreadable(const blocks::ValidationHooks* hooks, const char* field,
                                               std::uint64_t found, std::uint64_t expected,
                                               const char* why) noexcept {
    if (hooks != nullptr && hooks->diagnostic != nullptr) *hooks->diagnostic = why;
    return {blocks::Check::CONFORMANCE, "TILE_PIXEL_DATA", field, found, expected, 0};
}

/// f(n) and uvlc() over one OBU's payload, most significant bit first
/// (section 4.10.2). The first element that runs past the payload sticks:
/// every read after it returns zero, so the syntax below reads as
/// straight-line code and the caller reports that one element.
class SyntaxReader {
public:
    SyntaxReader(const BYTE* payload, Size size) noexcept : _payload(payload), _size(size) {}

    /// f(bits), bits <= 32: the next element, or zero past the payload's end.
    std::uint64_t f(const char* field, unsigned bits) noexcept {
        if (_ended != nullptr) return 0;
        // Never `_bit + bits > _size * 8`: subtract from the bound instead.
        if (bits > _size * 8 - _bit) {
            _ended = field;
            return 0;
        }
        std::uint64_t value = 0;
        for (unsigned i = 0; i < bits; ++i, ++_bit)
            value = value << 1 | ((_payload[_bit / 8] >> (7 - _bit % 8)) & 1u);
        return value;
    }

    /// uvlc() (section 4.10.3): a run of zero bits, a one, then that many bits.
    /// The run is bounded by the payload; at 32 zeros or more no bits follow.
    void uvlc(const char* field) noexcept {
        constexpr unsigned MAX_LEADING_ZEROS = 32;
        unsigned zeros = 0;
        for (;;) {
            const std::uint64_t done = f(field, 1);
            if (_ended != nullptr || done != 0) break;
            ++zeros;
        }
        if (zeros < MAX_LEADING_ZEROS) f(field, zeros);
    }

    /// The element the payload ended inside, or null.
    [[nodiscard]] const char* ended() const noexcept { return _ended; }

private:
    const BYTE*   _payload;
    Size          _size;
    std::uint64_t _bit   = 0;
    const char*   _ended = nullptr;
};

/// decoder_model_present_for_this_op and operating_parameters_info() of one
/// operating point.
inline void skip_operating_parameters(SyntaxReader& r, unsigned delay_bits) noexcept {
    const bool for_this_op = r.f("decoder_model_present_for_this_op", 1) != 0;
    if (!for_this_op) return;
    r.f("decoder_buffer_delay", delay_bits);
    r.f("encoder_buffer_delay", delay_bits);
    r.f("low_delay_mode_flag", 1);
}

/// The timing, decoder-model and operating-point elements of a sequence
/// header without the reduced still-picture form, read to reach the frame
/// size after them. operating_points_cnt_minus_1 is 5 bits, so the loop runs
/// at most 32 times.
inline void skip_operating_points(SyntaxReader& r) noexcept {
    const bool timing_info   = r.f("timing_info_present_flag", 1) != 0;
    bool       decoder_model = false;
    unsigned   delay_bits    = 0;
    if (timing_info) {
        r.f("num_units_in_display_tick", 32);
        r.f("time_scale", 32);
        const bool equal_interval = r.f("equal_picture_interval", 1) != 0;
        if (equal_interval) r.uvlc("num_ticks_per_picture_minus_1");
        decoder_model = r.f("decoder_model_info_present_flag", 1) != 0;
    }
    if (decoder_model) {
        delay_bits = static_cast<unsigned>(r.f("buffer_delay_length_minus_1", 5)) + 1;
        r.f("num_units_in_decoding_tick", 32);
        r.f("buffer_removal_time_length_minus_1", 5);
        r.f("frame_presentation_time_length_minus_1", 5);
    }
    const bool          initial_display_delay = r.f("initial_display_delay_present_flag", 1) != 0;
    const std::uint64_t points                = r.f("operating_points_cnt_minus_1", 5) + 1;
    for (std::uint64_t i = 0; i < points && r.ended() == nullptr; ++i) {
        r.f("operating_point_idc", 12);
        const std::uint64_t level = r.f("seq_level_idx", 5);
        if (level > LAST_LEVEL_WITHOUT_TIER) r.f("seq_tier", 1);
        if (decoder_model) skip_operating_parameters(r, delay_bits);
        if (initial_display_delay) {
            const bool delay_for_op = r.f("initial_display_delay_present_for_this_op", 1) != 0;
            if (delay_for_op) r.f("initial_display_delay_minus_1", 4);
        }
    }
}

/// The inter-prediction tool flags of a sequence header without the reduced
/// still-picture form, read to reach enable_superres after them.
inline void skip_inter_tools(SyntaxReader& r) noexcept {
    r.f("enable_interintra_compound", 1);
    r.f("enable_masked_compound", 1);
    r.f("enable_warped_motion", 1);
    r.f("enable_dual_filter", 1);
    const bool order_hint = r.f("enable_order_hint", 1) != 0;
    if (order_hint) {
        r.f("enable_jnt_comp", 1);
        r.f("enable_ref_frame_mvs", 1);
    }
    std::uint64_t screen_content = SELECT_SCREEN_CONTENT_TOOLS;
    const bool    choose_screen  = r.f("seq_choose_screen_content_tools", 1) != 0;
    if (!choose_screen) screen_content = r.f("seq_force_screen_content_tools", 1);
    if (screen_content > 0) {
        const bool choose_integer_mv = r.f("seq_choose_integer_mv", 1) != 0;
        if (!choose_integer_mv) r.f("seq_force_integer_mv", 1);
    }
    if (order_hint) r.f("order_hint_bits_minus_1", 3);
}

/// color_config() (section 5.5.2) as far as color_range, with the values the
/// syntax infers where an element is not coded.
inline void read_color_config(SyntaxReader& r, blocks::Av1StreamFields& out) noexcept {
    out.high_bitdepth = r.f("high_bitdepth", 1);
    if (out.seq_profile == PROFILE_PROFESSIONAL && out.high_bitdepth != 0) r.f("twelve_bit", 1);
    out.mono_chrome = 0;
    if (out.seq_profile != PROFILE_444) out.mono_chrome = r.f("mono_chrome", 1);
    out.color_description_present_flag = r.f("color_description_present_flag", 1);
    std::uint64_t primaries = CP_UNSPECIFIED;
    std::uint64_t transfer  = TC_UNSPECIFIED;
    out.matrix_coefficients = MC_UNSPECIFIED;
    if (out.color_description_present_flag != 0) {
        primaries               = r.f("color_primaries", 8);
        transfer                = r.f("transfer_characteristics", 8);
        out.matrix_coefficients = r.f("matrix_coefficients", 8);
    }
    // The BT.709 / sRGB / identity branch codes no color_range: it is 1.
    const bool srgb = out.mono_chrome == 0 && primaries == CP_BT_709 && transfer == TC_SRGB &&
                      out.matrix_coefficients == MC_IDENTITY;
    out.color_range = FULL_SWING;
    if (!srgb) out.color_range = r.f("color_range", 1);
}

/// sequence_header_obu() (section 5.5.1) over its payload, as far as
/// color_config's color_range.
inline void read_sequence_header(SyntaxReader& r, blocks::Av1StreamFields& out) noexcept {
    out.seq_profile   = r.f("seq_profile", 3);
    out.still_picture = r.f("still_picture", 1);
    const bool reduced = r.f("reduced_still_picture_header", 1) != 0;
    if (reduced) r.f("seq_level_idx", 5);
    else skip_operating_points(r);
    const unsigned width_bits  = static_cast<unsigned>(r.f("frame_width_bits_minus_1", 4)) + 1;
    const unsigned height_bits = static_cast<unsigned>(r.f("frame_height_bits_minus_1", 4)) + 1;
    out.max_frame_width_minus_1  = r.f("max_frame_width_minus_1", width_bits);
    out.max_frame_height_minus_1 = r.f("max_frame_height_minus_1", height_bits);
    if (!reduced) {
        const bool frame_ids = r.f("frame_id_numbers_present_flag", 1) != 0;
        if (frame_ids) {
            r.f("delta_frame_id_length_minus_2", 4);
            r.f("additional_frame_id_length_minus_1", 3);
        }
    }
    r.f("use_128x128_superblock", 1);
    r.f("enable_filter_intra", 1);
    r.f("enable_intra_edge_filter", 1);
    if (!reduced) skip_inter_tools(r);
    out.enable_superres = r.f("enable_superres", 1);
    r.f("enable_cdef", 1);
    r.f("enable_restoration", 1);
    read_color_config(r, out);
}

/// leb128() (section 4.10.5) at `at`, advancing it past the bytes read.
[[nodiscard]] inline blocks::Status read_leb128(const BYTE* stream, Size size, Size& at, std::uint64_t& value,
                                                const blocks::ValidationHooks* hooks) noexcept {
    value = 0;
    for (unsigned i = 0; i < LEB128_MAX_BYTES; ++i) {
        if (at == size)
            return unreadable(hooks, "obu_size", size, 0, "TILE_PIXEL_DATA under TILE_ENCODING_AV1 ends inside an obu_size.");
        const unsigned byte = stream[at++];
        value |= std::uint64_t{byte & LEB128_PAYLOAD} << (i * 7);
        if ((byte & LEB128_MORE) == 0) break;
        if (i + 1 == LEB128_MAX_BYTES)
            return unreadable(hooks, "leb128_byte", byte, byte & LEB128_PAYLOAD,
                              "TILE_PIXEL_DATA under TILE_ENCODING_AV1 has an obu_size longer than the "
                              "8 bytes leb128 allows (AV1 section 4.10.5).");
    }
    if (value > LEB128_MAX)
        return unreadable(hooks, "obu_size", value, LEB128_MAX,
                          "TILE_PIXEL_DATA under TILE_ENCODING_AV1 has an obu_size above 2^32 - 1 "
                          "(AV1 section 4.10.5).");
    return {};
}

}  // namespace av1

[[nodiscard]] inline blocks::Status read_av1_stream(const BYTE* stream, Size size,
                                                    blocks::Av1StreamFields& fields,
                                                    const blocks::ValidationHooks* hooks) noexcept {
    // Every pass consumes at least the header byte, so the walk ends.
    Size at = 0;
    while (true) {
        if (at == size)
            return av1::unreadable(hooks, "sequence_header_obu", size, 0,
                                   "TILE_PIXEL_DATA under TILE_ENCODING_AV1 ends before its first "
                                   "sequence_header_obu.");
        const unsigned header = stream[at++];
        if ((header & av1::FORBIDDEN_BIT) != 0)
            return av1::unreadable(hooks, "obu_forbidden_bit", 1, 0,
                                   "TILE_PIXEL_DATA under TILE_ENCODING_AV1 has an OBU whose "
                                   "obu_forbidden_bit is set (AV1 section 6.2.2).");
        if ((header & av1::HAS_SIZE_FIELD) == 0)
            return av1::unreadable(hooks, "obu_has_size_field", 0, 1,
                                   "TILE_PIXEL_DATA under TILE_ENCODING_AV1 has an OBU with no obu_size, "
                                   "which the Low Overhead Bitstream Format requires (AV1 section 5.2).");
        if ((header & av1::EXTENSION_FLAG) != 0) {
            if (at == size)
                return av1::unreadable(hooks, "obu_extension_header", size, 0,
                                       "TILE_PIXEL_DATA under TILE_ENCODING_AV1 ends inside an "
                                       "obu_extension_header.");
            ++at;
        }
        std::uint64_t        obu_size = 0;
        const blocks::Status length   = av1::read_leb128(stream, size, at, obu_size, hooks);
        if (!length) return length;
        if (obu_size > size - at)
            return av1::unreadable(hooks, "obu_size", obu_size, size - at,
                                   "TILE_PIXEL_DATA under TILE_ENCODING_AV1 has an obu_size past the end "
                                   "of the tile byte-stream.");
        if (((header >> av1::TYPE_SHIFT) & av1::TYPE_MASK) != av1::OBU_SEQUENCE_HEADER) {
            at += obu_size;
            continue;
        }
        av1::SyntaxReader reader(stream + at, obu_size);
        blocks::Av1StreamFields read;
        av1::read_sequence_header(reader, read);
        if (reader.ended() != nullptr)
            return av1::unreadable(hooks, reader.ended(), obu_size, 0,
                                   "TILE_PIXEL_DATA under TILE_ENCODING_AV1 has a sequence_header_obu "
                                   "that ends before the elements external_check compares.");
        fields = read;
        return {};
    }
}

}  // namespace Iris::File::detail

#endif  // IFE_Av1_hpp

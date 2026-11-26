/**
 * @file IFE_Av1.hpp
 * @brief The one piece of AV1 syntax IFE reads: a TILE_ENCODING_AV1 tile's
 *        first sequence header, checked against the colour and frame-size
 *        rules of D31 (development/ARCHITECTURE.md; spec, AV1 Tile
 *        Byte-Streams).
 * @copyright Iris Developers, 2025-2026
 *
 * The rules: 8-bit 4:2:0 or 4:4:4 colour with the BT.601 matrix at full range,
 * frames of TILE_LENGTH square with superres off, no film grain, one
 * operating point. Every syntax element is named and read as the AV1
 * specification's tables write it (https://aomediacodec.github.io/av1-spec/,
 * read 2026-11-26): section 5.3 (OBU header), 4.10.5 (leb128), 4.10.3 (uvlc),
 * 5.5.1 (sequence_header_obu), 5.5.2 (color_config). Nothing after the
 * sequence header is read: no frame header, no tile data.
 *
 * Every read is bounded by the bytes it was given: an OBU by the stream, a
 * field by its OBU. The check is noexcept and allocation-free and returns a
 * finding instead of throwing, as the generated validators return a Status,
 * so the opt-in validation layer can call it; `validate_file_structure`
 * formats the finding.
 *
 * Private: in namespace `detail`, included by src/IFE_Runtime.cpp and the
 * generated IFE_Validation.cpp so the library and the layer run one parser.
 * CMake does not install it (IFE_SourcesExport leaves it out); Bazel declares
 * it in :src_headers, since its sandbox hides an undeclared header.
 */
#ifndef IFE_Av1_hpp
#define IFE_Av1_hpp

#include <cstdint>

#include "IFE_Bytes.hpp"   // BYTE, Size

namespace Iris::File::detail {

/// How a TILE_ENCODING_AV1 tile stream fails its check.
enum class Av1Fault : std::uint8_t {
    NONE,          ///< the first sequence header meets every rule
    MUST_EQUAL,    ///< `field` holds `found` where `expected` is required
    ABOVE_LIMIT,   ///< `field` holds `found`, above the largest value allowed, `expected`
    PAST_END,      ///< `field` claims `found` bytes where `expected` remain in the stream
    ENDS_INSIDE,   ///< the stream or OBU, `found` bytes long, ends inside `field`
};

/// What the check found, unformatted. `field` names the AV1 syntax element.
struct Av1Finding {
    Av1Fault      fault    = Av1Fault::NONE;
    const char*   field    = "";
    std::uint64_t found    = 0;
    std::uint64_t expected = 0;

    constexpr explicit operator bool() const noexcept { return fault == Av1Fault::NONE; }
};

/**
 * @brief Check a TILE_ENCODING_AV1 tile byte-stream's first sequence header.
 *
 * Temporal delimiter, padding and reserved OBUs before it are skipped, as a
 * decoder skips them; any other OBU before it is refused.
 *
 * Preconditions: @p stream addresses @p size readable bytes (it may be null
 * only when @p size is 0); @p tile_length is the slide's tile edge in pixels,
 * resolved from TILE_LENGTH (256 where it is zero).
 */
[[nodiscard]] inline Av1Finding check_av1_tile_stream(const BYTE* stream, Size size,
                                                      std::uint16_t tile_length) noexcept;

// ============================================================================
// Implementation
// ============================================================================

namespace av1 {

// The OBU header byte (section 5.3.2): obu_forbidden_bit, obu_type (4 bits),
// obu_extension_flag, obu_has_size_field, obu_reserved_1bit.
inline constexpr unsigned FORBIDDEN_BIT  = 0x80;
inline constexpr unsigned TYPE_SHIFT     = 3;
inline constexpr unsigned TYPE_MASK      = 0x0F;
inline constexpr unsigned EXTENSION_FLAG = 0x04;
inline constexpr unsigned HAS_SIZE_FIELD = 0x02;

// obu_type (section 6.2.2).
inline constexpr unsigned OBU_RESERVED_0         = 0;
inline constexpr unsigned OBU_SEQUENCE_HEADER    = 1;
inline constexpr unsigned OBU_TEMPORAL_DELIMITER = 2;
inline constexpr unsigned OBU_RESERVED_FIRST     = 9;
inline constexpr unsigned OBU_RESERVED_LAST      = 14;
inline constexpr unsigned OBU_PADDING            = 15;

// leb128() (section 4.10.5): at most 8 bytes, a value at most 2^32 - 1.
inline constexpr unsigned      LEB128_MAX_BYTES = 8;
inline constexpr unsigned      LEB128_MORE      = 0x80;
inline constexpr unsigned      LEB128_PAYLOAD   = 0x7F;
inline constexpr std::uint64_t LEB128_MAX       = 0xFFFFFFFFu;

// sequence_header_obu() (section 5.5.1).
inline constexpr unsigned      LAST_LEVEL_WITHOUT_TIER     = 7;   // seq_tier is coded above it
inline constexpr std::uint64_t SELECT_SCREEN_CONTENT_TOOLS = 2;

// What D31 requires.
inline constexpr std::uint64_t HIGHEST_PROFILE = 1;   // 0 is 4:2:0, 1 is 4:4:4, both 8-bit
inline constexpr std::uint64_t PROFILE_444     = 1;   // mono_chrome is not coded (section 5.5.2)
inline constexpr std::uint64_t PROFILE_420     = 0;   // chroma_sample_position is coded
inline constexpr std::uint64_t MC_BT_601       = 6;   // matrix_coefficients (section 6.4.2)
inline constexpr std::uint64_t FULL_SWING      = 1;   // color_range

/// Whether a decoder skips this OBU type before a sequence header: a
/// temporal delimiter opens the temporal unit (section 7.5), padding and
/// reserved OBUs are ignored (section 6.2.2).
[[nodiscard]] constexpr bool is_skipped(unsigned type) noexcept {
    return type == OBU_TEMPORAL_DELIMITER || type == OBU_PADDING || type == OBU_RESERVED_0 ||
           (type >= OBU_RESERVED_FIRST && type <= OBU_RESERVED_LAST);
}

/// f(n) and uvlc() over one OBU's payload, most significant bit first
/// (section 4.10.2). The first finding sticks: every read after it returns
/// zero and every requirement after it is ignored, so the syntax below reads
/// as straight-line code and reports the first field that failed.
class SyntaxReader {
public:
    SyntaxReader(const BYTE* payload, Size size) noexcept : _payload(payload), _size(size) {}

    /// f(bits), bits <= 32: the next field, or zero past the payload's end.
    std::uint64_t f(const char* field, unsigned bits) noexcept {
        if (!_finding) return 0;
        // Never `_bit + bits > _size * 8`: subtract from the bound instead.
        if (bits > _size * 8 - _bit) {
            _finding = {Av1Fault::ENDS_INSIDE, field, _size, 0};
            return 0;
        }
        std::uint64_t value = 0;
        for (unsigned i = 0; i < bits; ++i, ++_bit)
            value = value << 1 | ((_payload[_bit / 8] >> (7 - _bit % 8)) & 1u);
        return value;
    }

    /// uvlc() (section 4.10.3): a run of zero bits, a one, then that many bits.
    /// The run is bounded by the payload; at 32 zeros or more the value is
    /// 2^32 - 1 and no bits follow.
    void uvlc(const char* field) noexcept {
        constexpr unsigned MAX_LEADING_ZEROS = 32;
        unsigned zeros = 0;
        for (;;) {
            const std::uint64_t done = f(field, 1);
            if (!_finding || done != 0) break;
            ++zeros;
        }
        if (zeros < MAX_LEADING_ZEROS) f(field, zeros);
    }

    /// `field`, already read as `value`, must equal `expected`.
    void require(const char* field, std::uint64_t value, std::uint64_t expected) noexcept {
        if (_finding && value != expected) _finding = {Av1Fault::MUST_EQUAL, field, value, expected};
    }

    /// `field`, already read as `value`, may be at most `limit`.
    void require_at_most(const char* field, std::uint64_t value, std::uint64_t limit) noexcept {
        if (_finding && value > limit) _finding = {Av1Fault::ABOVE_LIMIT, field, value, limit};
    }

    [[nodiscard]] Av1Finding finding() const noexcept { return _finding; }

private:
    const BYTE*   _payload;
    Size          _size;
    std::uint64_t _bit = 0;
    Av1Finding    _finding;
};

/// leb128() (section 4.10.5) at `at`, advancing it past the bytes read.
[[nodiscard]] inline Av1Finding read_leb128(const BYTE* stream, Size size, Size& at,
                                            std::uint64_t& value) noexcept {
    value = 0;
    for (unsigned i = 0; i < LEB128_MAX_BYTES; ++i) {
        if (at == size) return {Av1Fault::ENDS_INSIDE, "obu_size", size, 0};
        const unsigned byte = stream[at++];
        value |= std::uint64_t{byte & LEB128_PAYLOAD} << (i * 7);
        if ((byte & LEB128_MORE) == 0) break;
        // The eighth byte ends the encoding (section 4.10.5).
        if (i + 1 == LEB128_MAX_BYTES) return {Av1Fault::MUST_EQUAL, "leb128_byte", byte, byte & LEB128_PAYLOAD};
    }
    if (value > LEB128_MAX) return {Av1Fault::ABOVE_LIMIT, "obu_size", value, LEB128_MAX};
    return {};
}

/// The timing, decoder-model and operating-point fields of a sequence header
/// without the reduced still-picture form. One operating point, idc 0, is
/// required, so a decoder outputs every frame (section 7.18.1).
inline void read_operating_points(SyntaxReader& r) noexcept {
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
    const bool initial_display_delay = r.f("initial_display_delay_present_flag", 1) != 0;
    const std::uint64_t points = r.f("operating_points_cnt_minus_1", 5);
    r.require("operating_points_cnt_minus_1", points, 0);
    // One operating point, so the syntax's loop runs once.
    const std::uint64_t idc = r.f("operating_point_idc", 12);
    r.require("operating_point_idc", idc, 0);
    const std::uint64_t level = r.f("seq_level_idx", 5);
    if (level > LAST_LEVEL_WITHOUT_TIER) r.f("seq_tier", 1);
    if (decoder_model) {
        const bool for_this_op = r.f("decoder_model_present_for_this_op", 1) != 0;
        if (for_this_op) {
            r.f("decoder_buffer_delay", delay_bits);
            r.f("encoder_buffer_delay", delay_bits);
            r.f("low_delay_mode_flag", 1);
        }
    }
    if (initial_display_delay) {
        const bool for_this_op = r.f("initial_display_delay_present_for_this_op", 1) != 0;
        if (for_this_op) r.f("initial_display_delay_minus_1", 4);
    }
}

/// The inter-prediction tool flags of a sequence header without the reduced
/// still-picture form. None is constrained; each is read to reach the fields
/// after it.
inline void read_inter_tools(SyntaxReader& r) noexcept {
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
    const bool choose_screen_content = r.f("seq_choose_screen_content_tools", 1) != 0;
    if (!choose_screen_content) screen_content = r.f("seq_force_screen_content_tools", 1);
    if (screen_content > 0) {
        const bool choose_integer_mv = r.f("seq_choose_integer_mv", 1) != 0;
        if (!choose_integer_mv) r.f("seq_force_integer_mv", 1);
    }
    if (order_hint) r.f("order_hint_bits_minus_1", 3);
}

/// color_config() (section 5.5.2) under D31. Its BT.709 / sRGB / identity
/// branch codes no color_range; it needs matrix_coefficients 0
/// (MC_IDENTITY), which the BT.601 requirement refuses before that branch.
///
/// Preconditions: `profile` is 0 or 1 unless the reader already holds a
/// finding, so twelve_bit (coded only under profile 2) is never present.
inline void read_color_config(SyntaxReader& r, std::uint64_t profile) noexcept {
    r.require("high_bitdepth", r.f("high_bitdepth", 1), 0);
    if (profile != PROFILE_444) r.require("mono_chrome", r.f("mono_chrome", 1), 0);
    r.require("color_description_present_flag", r.f("color_description_present_flag", 1), 1);
    // Colorimetry is the ICC profile's (spec, ICC Color Profile), so the
    // primaries and the transfer are read and left unconstrained.
    r.f("color_primaries", 8);
    r.f("transfer_characteristics", 8);
    r.require("matrix_coefficients", r.f("matrix_coefficients", 8), MC_BT_601);
    r.require("color_range", r.f("color_range", 1), FULL_SWING);
    if (profile == PROFILE_420) r.f("chroma_sample_position", 2);
    r.f("separate_uv_delta_q", 1);
}

/// sequence_header_obu() (section 5.5.1) over its payload.
[[nodiscard]] inline Av1Finding check_sequence_header(const BYTE* payload, Size size,
                                                      std::uint16_t tile_length) noexcept {
    SyntaxReader r(payload, size);
    const std::uint64_t profile = r.f("seq_profile", 3);
    r.require_at_most("seq_profile", profile, HIGHEST_PROFILE);
    r.f("still_picture", 1);
    const bool reduced = r.f("reduced_still_picture_header", 1) != 0;
    if (reduced) r.f("seq_level_idx", 5);
    else read_operating_points(r);

    // The sequence header bounds every frame; D31 makes the bound the size.
    const unsigned width_bits  = static_cast<unsigned>(r.f("frame_width_bits_minus_1", 4)) + 1;
    const unsigned height_bits = static_cast<unsigned>(r.f("frame_height_bits_minus_1", 4)) + 1;
    const std::uint64_t edge_minus_1 = std::uint64_t{tile_length} - 1;
    r.require("max_frame_width_minus_1", r.f("max_frame_width_minus_1", width_bits), edge_minus_1);
    r.require("max_frame_height_minus_1", r.f("max_frame_height_minus_1", height_bits), edge_minus_1);

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
    if (!reduced) read_inter_tools(r);
    // Superres would decode a narrower frame and upscale it (section 7.16).
    r.require("enable_superres", r.f("enable_superres", 1), 0);
    r.f("enable_cdef", 1);
    r.f("enable_restoration", 1);
    read_color_config(r, profile);
    // Grain synthesized at output (section 7.18.3) would change the RGB.
    r.require("film_grain_params_present", r.f("film_grain_params_present", 1), 0);
    return r.finding();
}

}  // namespace av1

[[nodiscard]] inline Av1Finding check_av1_tile_stream(const BYTE* stream, Size size,
                                                      std::uint16_t tile_length) noexcept {
    // Every pass consumes at least the header byte, so the walk ends.
    Size at = 0;
    while (true) {
        if (at == size) return {Av1Fault::ENDS_INSIDE, "obu_header", size, 0};
        const unsigned header = stream[at++];
        if ((header & av1::FORBIDDEN_BIT) != 0) return {Av1Fault::MUST_EQUAL, "obu_forbidden_bit", 1, 0};
        if ((header & av1::HAS_SIZE_FIELD) == 0) return {Av1Fault::MUST_EQUAL, "obu_has_size_field", 0, 1};
        const unsigned type      = (header >> av1::TYPE_SHIFT) & av1::TYPE_MASK;
        const bool     extension = (header & av1::EXTENSION_FLAG) != 0;
        // A sequence header is not layer-specific, so it carries no extension
        // header (section 6.2.2).
        if (extension && type == av1::OBU_SEQUENCE_HEADER)
            return {Av1Fault::MUST_EQUAL, "obu_extension_flag", 1, 0};
        if (extension) {
            if (at == size) return {Av1Fault::ENDS_INSIDE, "obu_extension_header", size, 0};
            ++at;
        }
        std::uint64_t  obu_size = 0;
        const Av1Finding length = av1::read_leb128(stream, size, at, obu_size);
        if (!length) return length;
        if (obu_size > size - at) return {Av1Fault::PAST_END, "obu_size", obu_size, size - at};
        if (type == av1::OBU_SEQUENCE_HEADER) return av1::check_sequence_header(stream + at, obu_size, tile_length);
        if (!av1::is_skipped(type)) return {Av1Fault::MUST_EQUAL, "obu_type", type, av1::OBU_SEQUENCE_HEADER};
        at += obu_size;
    }
}

}  // namespace Iris::File::detail

#endif  // IFE_Av1_hpp

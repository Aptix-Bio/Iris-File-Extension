/**
 * @file ife_av1_tile_stream_tests.cpp
 * @brief The colour and frame-size rules of TILE_ENCODING_AV1 tiles (TE-3,
 *        development/WBS.md; D31, development/ARCHITECTURE.md): each tile's
 *        first AV1 sequence header declares 8-bit 4:2:0 or 4:4:4 colour with
 *        the BT.601 matrix at full range, frames of exactly TILE_LENGTH square
 *        with superres off, and no film grain.
 *
 * What this pins, through the two places the rules run:
 *   1. The opt-in validation layer's `check_tile_stream`: a conformant
 *      sequence header on every branch of its syntax passes; each rule, each
 *      OBU framing requirement and each bound fails as CONFORMANCE naming the
 *      syntax element and its value; a stream cut at every byte fails without
 *      reading past it (run under ASan); seeded garbage returns; a tile
 *      encoding with no stream clause is not parsed.
 *   2. `validate_file_structure`: a Builder-written AV1 slide passes, and one
 *      tile that breaks a rule fails as CONFORMANCE naming the layer, the tile,
 *      the field and its value. `abstract_file_structure` does not parse tile
 *      streams (D14), so it still reads that slide.
 *
 * The sequence headers are hand-built with a bit writer that follows the AV1
 * specification's syntax tables (https://aomediacodec.github.io/av1-spec/,
 * read 2026-11-26): section 5.3 (OBU header), 4.10.5 (leb128), 4.10.3 (uvlc),
 * 5.5.1 (sequence_header_obu), 5.5.2 (color_config). Two headers written by
 * an independent encoder, libaom's aomenc, are checked as well.
 *
 * Self-contained (no framework, no corpus). Non-zero exit on failure.
 */
#include "IFE_Builder.hpp"
#include "IFE_Validation.hpp"
#include "IrisFileExtension.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace Iris;
using namespace Iris::File;

namespace b = ::Iris::File::blocks;
namespace k = ::Iris::File::constants;

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

// ---- AV1 values, as the AV1 specification names them ---------------------- //

// obu_type (section 6.2.2).
constexpr unsigned OBU_SEQUENCE_HEADER    = 1;
constexpr unsigned OBU_TEMPORAL_DELIMITER = 2;
constexpr unsigned OBU_METADATA           = 5;
constexpr unsigned OBU_FRAME              = 6;
constexpr unsigned OBU_RESERVED_9         = 9;
constexpr unsigned OBU_PADDING            = 15;
// color_primaries, transfer_characteristics, matrix_coefficients (section 6.4.2).
constexpr unsigned CP_BT_709      = 1;
constexpr unsigned CP_UNSPECIFIED = 2;
constexpr unsigned TC_UNSPECIFIED = 2;
constexpr unsigned TC_SRGB        = 13;
constexpr unsigned MC_IDENTITY    = 0;
constexpr unsigned MC_BT_709      = 1;
constexpr unsigned MC_BT_601      = 6;

constexpr std::uint16_t TILE = 256;   // the default tile edge (spec, Tile Length)

// ---- a bit writer and the syntax it writes -------------------------------- //

/// f(n) fields, most significant bit first (AV1 section 4.10.2).
class BitWriter {
public:
    void put(std::uint64_t value, unsigned bits) {
        for (unsigned i = bits; i-- > 0;) {
            if (_bit % 8 == 0) _bytes.push_back(0);
            if ((value >> i) & 1u) _bytes.back() |= static_cast<BYTE>(0x80u >> (_bit % 8));
            ++_bit;
        }
    }
    /// uvlc() (section 4.10.3): leading zeros, a one, then the value's low bits.
    void uvlc(std::uint32_t value) {
        const std::uint64_t biased = std::uint64_t{value} + 1;
        unsigned            width  = 0;
        while ((biased >> (width + 1)) != 0) ++width;
        put(0, width);
        put(biased, width + 1);
    }
    /// trailing_bits() (section 5.3.4): a one, then zeros to the byte boundary.
    std::vector<BYTE> finish() {
        put(1, 1);
        return std::move(_bytes);
    }

private:
    std::vector<BYTE> _bytes;
    unsigned          _bit = 0;
};

/// Every field of sequence_header_obu() (section 5.5.1) a test varies; the
/// defaults are a conformant 256-pixel 4:2:0 header without the reduced form.
struct SequenceHeader {
    unsigned seq_profile                  = 0;
    unsigned still_picture                = 0;
    unsigned reduced_still_picture_header = 0;
    unsigned timing_info_present_flag     = 0;
    unsigned equal_picture_interval       = 0;
    unsigned decoder_model_info_present   = 0;
    unsigned decoder_model_present_for_op = 0;
    unsigned initial_display_delay_present = 0;
    unsigned initial_display_delay_for_op = 0;
    unsigned operating_points_cnt_minus_1 = 0;
    unsigned operating_point_idc          = 0;
    unsigned seq_level_idx                = 8;   // above 7, so seq_tier is coded
    unsigned frame_width_bits_minus_1     = 15;
    unsigned frame_height_bits_minus_1    = 15;
    unsigned frame_width                  = TILE;
    unsigned frame_height                 = TILE;
    unsigned frame_id_numbers_present     = 0;
    unsigned enable_order_hint            = 1;
    unsigned seq_choose_screen_content    = 1;
    unsigned seq_force_screen_content     = 0;
    unsigned seq_choose_integer_mv        = 1;
    unsigned enable_superres              = 0;
    unsigned high_bitdepth                = 0;
    unsigned mono_chrome                  = 0;
    unsigned color_description_present    = 1;
    unsigned color_primaries              = CP_BT_709;
    unsigned transfer_characteristics     = TC_SRGB;
    unsigned matrix_coefficients          = MC_BT_601;
    unsigned color_range                  = 1;
    unsigned film_grain_params_present    = 0;
};

/// The payload of sequence_header_obu(), field by field in syntax order.
std::vector<BYTE> sequence_header_payload(const SequenceHeader& s) {
    BitWriter w;
    w.put(s.seq_profile, 3);
    w.put(s.still_picture, 1);
    w.put(s.reduced_still_picture_header, 1);
    const bool reduced = s.reduced_still_picture_header != 0;
    if (reduced) {
        w.put(s.seq_level_idx & 0x1F, 5);
    } else {
        w.put(s.timing_info_present_flag, 1);
        if (s.timing_info_present_flag) {
            w.put(1, 32);                          // num_units_in_display_tick
            w.put(25, 32);                         // time_scale
            w.put(s.equal_picture_interval, 1);
            if (s.equal_picture_interval) w.uvlc(0);
            w.put(s.decoder_model_info_present, 1);
            if (s.decoder_model_info_present) {
                w.put(9, 5);                       // buffer_delay_length_minus_1: n = 10
                w.put(1, 32);                      // num_units_in_decoding_tick
                w.put(4, 5);                       // buffer_removal_time_length_minus_1
                w.put(4, 5);                       // frame_presentation_time_length_minus_1
            }
        }
        w.put(s.initial_display_delay_present, 1);
        w.put(s.operating_points_cnt_minus_1, 5);
        for (unsigned i = 0; i <= s.operating_points_cnt_minus_1; ++i) {
            w.put(s.operating_point_idc, 12);
            w.put(s.seq_level_idx, 5);
            if (s.seq_level_idx > 7) w.put(0, 1);  // seq_tier
            if (s.timing_info_present_flag && s.decoder_model_info_present) {
                w.put(s.decoder_model_present_for_op, 1);
                if (s.decoder_model_present_for_op) {
                    w.put(0x155, 10);              // decoder_buffer_delay
                    w.put(0x0AA, 10);              // encoder_buffer_delay
                    w.put(1, 1);                   // low_delay_mode_flag
                }
            }
            if (s.initial_display_delay_present) {
                w.put(s.initial_display_delay_for_op, 1);
                if (s.initial_display_delay_for_op) w.put(9, 4);
            }
        }
    }
    w.put(s.frame_width_bits_minus_1, 4);
    w.put(s.frame_height_bits_minus_1, 4);
    w.put(s.frame_width - 1, s.frame_width_bits_minus_1 + 1);
    w.put(s.frame_height - 1, s.frame_height_bits_minus_1 + 1);
    if (!reduced) {
        w.put(s.frame_id_numbers_present, 1);
        if (s.frame_id_numbers_present) {
            w.put(13, 4);                          // delta_frame_id_length_minus_2
            w.put(2, 3);                           // additional_frame_id_length_minus_1
        }
    }
    w.put(0, 1);                                   // use_128x128_superblock
    w.put(1, 1);                                   // enable_filter_intra
    w.put(1, 1);                                   // enable_intra_edge_filter
    if (!reduced) {
        w.put(0b1010, 4);                          // interintra, masked, warped, dual filter
        w.put(s.enable_order_hint, 1);
        if (s.enable_order_hint) w.put(0b11, 2);   // enable_jnt_comp, enable_ref_frame_mvs
        w.put(s.seq_choose_screen_content, 1);
        if (!s.seq_choose_screen_content) w.put(s.seq_force_screen_content, 1);
        if (s.seq_choose_screen_content || s.seq_force_screen_content) {
            w.put(s.seq_choose_integer_mv, 1);
            if (!s.seq_choose_integer_mv) w.put(1, 1);   // seq_force_integer_mv
        }
        if (s.enable_order_hint) w.put(6, 3);      // order_hint_bits_minus_1
    }
    w.put(s.enable_superres, 1);
    w.put(1, 1);                                   // enable_cdef
    w.put(1, 1);                                   // enable_restoration
    // color_config() (section 5.5.2).
    w.put(s.high_bitdepth, 1);
    if (s.seq_profile == 2 && s.high_bitdepth) w.put(0, 1);   // twelve_bit
    if (s.seq_profile != 1) w.put(s.mono_chrome, 1);
    w.put(s.color_description_present, 1);
    if (s.color_description_present) {
        w.put(s.color_primaries, 8);
        w.put(s.transfer_characteristics, 8);
        w.put(s.matrix_coefficients, 8);
    }
    const bool srgb = s.color_primaries == CP_BT_709 && s.transfer_characteristics == TC_SRGB &&
                      s.matrix_coefficients == MC_IDENTITY && s.color_description_present;
    if (s.mono_chrome) {
        w.put(s.color_range, 1);
    } else if (!srgb) {
        w.put(s.color_range, 1);
        if (s.seq_profile == 0) w.put(0, 2);       // chroma_sample_position
    }
    if (!s.mono_chrome) w.put(0, 1);               // separate_uv_delta_q
    w.put(s.film_grain_params_present, 1);
    return w.finish();
}

/// leb128() (section 4.10.5), in the fewest bytes.
void put_leb128(std::vector<BYTE>& out, std::uint64_t value) {
    do {
        BYTE byte = value & 0x7F;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        out.push_back(byte);
    } while (value != 0);
}

/// One OBU (section 5.3.1): its header byte, an extension byte when asked
/// for, obu_size, the payload.
std::vector<BYTE> obu(unsigned type, const std::vector<BYTE>& payload, bool extension = false) {
    std::vector<BYTE> out{static_cast<BYTE>(type << 3 | (extension ? 0x04u : 0u) | 0x02u)};
    if (extension) out.push_back(0);
    put_leb128(out, payload.size());
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

/// A temporal unit's opening: a temporal delimiter, the sequence header, and
/// a frame OBU of filler, which the check never reads.
std::vector<BYTE> tile_stream(const SequenceHeader& s) {
    std::vector<BYTE> out = obu(OBU_TEMPORAL_DELIMITER, {});
    const std::vector<BYTE> header = obu(OBU_SEQUENCE_HEADER, sequence_header_payload(s));
    out.insert(out.end(), header.begin(), header.end());
    const std::vector<BYTE> frame = obu(OBU_FRAME, std::vector<BYTE>(24, 0xCD));
    out.insert(out.end(), frame.begin(), frame.end());
    return out;
}

// ---- the validation layer ------------------------------------------------- //

std::string g_diagnostic;

b::Status check(const std::vector<BYTE>& stream, std::uint16_t length = TILE,
                k::TileEncodings encoding = k::TileEncodings::TILE_ENCODING_AV1) {
    b::ValidationHooks hooks = b::conformance_layer();
    hooks.diagnostic = &g_diagnostic;
    g_diagnostic.clear();
    return b::check_tile_stream(encoding, length, stream.data(), stream.size(), 0, &hooks);
}

/// A CONFORMANCE failure on @p field, storing @p found.
bool refuses(const b::Status& status, const std::string& field, std::uint64_t found) {
    const bool matched = !status && status.code == b::Check::CONFORMANCE &&
                         std::string(status.block) == "TILE_PIXEL_DATA" &&
                         std::string(status.field) == field && status.found == found &&
                         !g_diagnostic.empty();
    if (!matched)
        std::fprintf(stderr, "  expected %s %llu, got %s %llu (code %d)\n", field.c_str(),
                     static_cast<unsigned long long>(found), status.field,
                     static_cast<unsigned long long>(status.found), static_cast<int>(status.code));
    return matched;
}

void test_conformant_headers_pass() {
    // Every branch of the syntax a conformant header can take.
    std::vector<SequenceHeader> headers(10);
    headers[1].seq_profile = 1;                                  // 4:4:4
    headers[2].reduced_still_picture_header = 1;                 // one still frame
    headers[2].still_picture = 1;
    headers[3].timing_info_present_flag = 1;                     // timing, decoder model, delays
    headers[3].equal_picture_interval = 1;
    headers[3].decoder_model_info_present = 1;
    headers[3].decoder_model_present_for_op = 1;
    headers[3].initial_display_delay_present = 1;
    headers[3].initial_display_delay_for_op = 1;
    headers[4].timing_info_present_flag = 1;                     // timing alone
    headers[4].initial_display_delay_present = 1;
    headers[5].enable_order_hint = 0;
    headers[5].seq_choose_screen_content = 0;                    // screen content forced off
    headers[6].seq_choose_screen_content = 0;                    // forced on, integer MV forced
    headers[6].seq_force_screen_content = 1;
    headers[6].seq_choose_integer_mv = 0;
    headers[7].frame_id_numbers_present = 1;
    headers[7].seq_level_idx = 4;                                // no seq_tier
    headers[8].frame_width_bits_minus_1 = 7;                     // the narrowest width field for 256
    headers[8].frame_height_bits_minus_1 = 7;
    headers[9].color_primaries = CP_UNSPECIFIED;                 // colorimetry is the ICC profile's
    headers[9].transfer_characteristics = TC_UNSPECIFIED;
    for (std::size_t i = 0; i < headers.size(); ++i) {
        const b::Status status = check(tile_stream(headers[i]));
        if (!status) std::fprintf(stderr, "  header %zu: %s %llu\n", i, status.field,
                                  static_cast<unsigned long long>(status.found));
        IFE_CHECK(static_cast<bool>(status));
        IFE_CHECK(g_diagnostic.empty());
    }

    // A 512-pixel slide's tiles are 512 square.
    SequenceHeader large;
    large.frame_width = large.frame_height = 512;
    IFE_CHECK(static_cast<bool>(check(tile_stream(large), 512)));

    // The sequence header may be the first OBU, or follow padding and
    // reserved OBUs, which a decoder ignores (sections 5.3.1, 6.2.2).
    std::vector<BYTE> skipped = obu(OBU_PADDING, {0x80});
    const std::vector<BYTE> reserved = obu(OBU_RESERVED_9, {0x01, 0x80});
    skipped.insert(skipped.end(), reserved.begin(), reserved.end());
    const std::vector<BYTE> header = obu(OBU_SEQUENCE_HEADER, sequence_header_payload({}));
    skipped.insert(skipped.end(), header.begin(), header.end());
    IFE_CHECK(static_cast<bool>(check(skipped)));
    IFE_CHECK(static_cast<bool>(check(header)));
}

/// Three headers libaom 3.13.2 wrote (`aomenc --obu`, a 256 x 256 Y4M input),
/// as an encoder independent of the bit writer above lays them out: each a
/// temporal delimiter, then the sequence header OBU.
void test_headers_an_encoder_wrote() {
    // 4:2:0 input marked XCOLORRANGE=FULL, --limit=2 --matrix-coefficients=bt601
    // --color-primaries=bt709 --transfer-characteristics=srgb: the full
    // sequence header form, conformant.
    const std::vector<BYTE> bt601 = {0x12, 0x00, 0x0A, 0x0E, 0x00, 0x00, 0x00, 0x03, 0xBF,
                                     0xFF, 0xFB, 0xFF, 0xF3, 0x20, 0x21, 0xA0, 0xD0, 0x80};
    IFE_CHECK(static_cast<bool>(check(bt601)));
    // The same input, --limit=1 and no colour flags: a reduced still-picture
    // header with no colour description.
    const std::vector<BYTE> plain = {0x12, 0x00, 0x0A, 0x07, 0x18, 0x1D, 0xFF,
                                     0xFF, 0xFB, 0x10, 0x80};
    IFE_CHECK(refuses(check(plain), "color_description_present_flag", 0));
    // 4:4:4 input, --limit=1 --profile=1 --i444 --matrix-coefficients=bt601:
    // seq_profile 1 with the BT.601 matrix, written at studio swing.
    const std::vector<BYTE> studio = {0x12, 0x00, 0x0A, 0x09, 0x38, 0x1D, 0xFF,
                                      0xFF, 0xFB, 0x40, 0x80, 0x81, 0x84};
    IFE_CHECK(refuses(check(studio), "color_range", 0));
}

/// Each rule of D31, broken alone, names its field and the value found.
void test_each_rule_names_its_field() {
    struct Case {
        SequenceHeader header;
        const char*    field;
        std::uint64_t  found;
    };
    std::vector<Case> cases;
    auto add = [&cases](const char* field, std::uint64_t found, auto&& mutate) {
        SequenceHeader s;
        mutate(s);
        cases.push_back({s, field, found});
    };
    add("seq_profile", 2, [](SequenceHeader& s) { s.seq_profile = 2; });
    add("high_bitdepth", 1, [](SequenceHeader& s) { s.high_bitdepth = 1; });
    add("mono_chrome", 1, [](SequenceHeader& s) { s.mono_chrome = 1; });
    add("color_description_present_flag", 0, [](SequenceHeader& s) { s.color_description_present = 0; });
    add("matrix_coefficients", MC_BT_709, [](SequenceHeader& s) { s.matrix_coefficients = MC_BT_709; });
    // BT.709 primaries, sRGB transfer and the identity matrix: color_range is
    // not coded (section 5.5.2), and the matrix fails first.
    add("matrix_coefficients", MC_IDENTITY, [](SequenceHeader& s) { s.matrix_coefficients = MC_IDENTITY; });
    add("color_range", 0, [](SequenceHeader& s) { s.color_range = 0; });
    add("max_frame_width_minus_1", 511, [](SequenceHeader& s) { s.frame_width = 512; });
    add("max_frame_height_minus_1", 127, [](SequenceHeader& s) { s.frame_height = 128; });
    add("enable_superres", 1, [](SequenceHeader& s) { s.enable_superres = 1; });
    add("film_grain_params_present", 1, [](SequenceHeader& s) { s.film_grain_params_present = 1; });
    add("operating_points_cnt_minus_1", 1, [](SequenceHeader& s) { s.operating_points_cnt_minus_1 = 1; });
    add("operating_point_idc", 0x101, [](SequenceHeader& s) { s.operating_point_idc = 0x101; });
    for (const Case& c : cases)
        IFE_CHECK(refuses(check(tile_stream(c.header)), c.field, c.found));

    // The frame size is checked against the slide's TILE_LENGTH.
    IFE_CHECK(refuses(check(tile_stream({}), 512), "max_frame_width_minus_1", TILE - 1));
}

/// The OBU framing and every bound on a length the stream claims.
void test_framing_and_bounds() {
    const std::vector<BYTE> header = obu(OBU_SEQUENCE_HEADER, sequence_header_payload({}));

    std::vector<BYTE> forbidden = header;
    forbidden[0] |= 0x80;
    IFE_CHECK(refuses(check(forbidden), "obu_forbidden_bit", 1));

    std::vector<BYTE> no_size = header;
    no_size[0] &= static_cast<BYTE>(~0x02u);
    IFE_CHECK(refuses(check(no_size), "obu_has_size_field", 0));

    // A sequence header is not layer-specific: no extension byte (section 6.2.2).
    IFE_CHECK(refuses(check(obu(OBU_SEQUENCE_HEADER, sequence_header_payload({}), true)),
                      "obu_extension_flag", 1));

    // An OBU other than a temporal delimiter, padding or a reserved type
    // before the sequence header.
    std::vector<BYTE> metadata_first = obu(OBU_METADATA, {0x01, 0x80});
    metadata_first.insert(metadata_first.end(), header.begin(), header.end());
    IFE_CHECK(refuses(check(metadata_first), "obu_type", OBU_METADATA));

    // obu_size past the bytes that remain.
    std::vector<BYTE> overrun = header;
    overrun[1] = static_cast<BYTE>(header.size());   // one more than the payload holds
    IFE_CHECK(refuses(check(overrun), "obu_size", header.size()));

    // leb128: above 2^32 - 1, and a continuation bit on the eighth byte.
    const std::vector<BYTE> wide = {0x0A, 0x80, 0x80, 0x80, 0x80, 0x10};   // 2^32
    IFE_CHECK(refuses(check(wide), "obu_size", std::uint64_t{1} << 32));
    const std::vector<BYTE> long_leb = {0x0A, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
    IFE_CHECK(refuses(check(long_leb), "leb128_byte", 0x80));

    // A stream with no OBU at all, and one holding only a temporal delimiter.
    IFE_CHECK(refuses(check({}), "obu_header", 0));
    IFE_CHECK(refuses(check(obu(OBU_TEMPORAL_DELIMITER, {})), "obu_header", 2));
}

/// Every prefix of a conformant stream that stops before the sequence
/// header's last byte fails, and reads nothing past its end: each prefix is
/// its own allocation, so AddressSanitizer catches a read one byte over.
void test_truncation_at_every_byte() {
    SequenceHeader s;
    s.timing_info_present_flag = 1;
    s.equal_picture_interval = 1;
    s.decoder_model_info_present = 1;
    s.decoder_model_present_for_op = 1;
    s.initial_display_delay_present = 1;
    s.initial_display_delay_for_op = 1;
    const std::vector<BYTE> stream = tile_stream(s);
    const std::size_t header_end = 2 + 2 + sequence_header_payload(s).size();   // TD, then the header OBU
    for (std::size_t n = 0; n < header_end; ++n) {
        const std::vector<BYTE> prefix(stream.begin(), stream.begin() + static_cast<std::ptrdiff_t>(n));
        const b::Status status = check(prefix);
        if (status) std::fprintf(stderr, "  a %zu-byte prefix passed\n", n);
        IFE_CHECK(!status && status.code == b::Check::CONFORMANCE);
    }
    const std::vector<BYTE> whole(stream.begin(), stream.begin() + static_cast<std::ptrdiff_t>(header_end));
    IFE_CHECK(static_cast<bool>(check(whole)));

    // A header whose obu_size agrees with a payload cut short: the syntax
    // runs out of bits inside the OBU. trailing_bits() fills at most the last
    // byte, so every payload two or more bytes short loses a coded field.
    const std::vector<BYTE> payload = sequence_header_payload(s);
    for (std::size_t n = 0; n + 2 <= payload.size(); ++n) {
        const std::vector<BYTE> cut(payload.begin(), payload.begin() + static_cast<std::ptrdiff_t>(n));
        const b::Status status = check(obu(OBU_SEQUENCE_HEADER, cut));
        if (status) std::fprintf(stderr, "  a %zu-byte payload passed\n", n);
        IFE_CHECK(!status && status.code == b::Check::CONFORMANCE);
    }
}

/// Seeded garbage, and seeded damage to a conformant stream, each in a buffer
/// of its own exact size: the check returns, under ASan and UBSan, whatever
/// the bytes. std::mt19937's output is fixed by the standard; its
/// distributions are not, so the bytes are taken from it directly.
void test_garbage_returns() {
    std::mt19937 rng(0xA71u);
    const std::vector<BYTE> clean = tile_stream({});
    std::size_t passed = 0;
    for (int trial = 0; trial < 20000; ++trial) {
        std::vector<BYTE> bytes;
        if (trial % 2 == 0) {
            bytes.resize(rng() % 48);
            for (BYTE& byte : bytes) byte = static_cast<BYTE>(rng());
        } else {
            bytes = clean;
            for (unsigned flips = 1 + rng() % 4; flips-- > 0;)
                bytes[rng() % bytes.size()] ^= static_cast<BYTE>(1u << (rng() % 8));
        }
        const b::Status status = check(bytes);
        IFE_CHECK(status || status.code == b::Check::CONFORMANCE);
        passed += static_cast<bool>(status);
    }
    std::printf("garbage: %zu of 20000 seeded inputs passed\n", passed);
}

/// The layer parses only what TILE_ENCODING_AV1 defines.
void test_other_encodings_are_not_parsed() {
    const std::vector<BYTE> filler(24, 0xCD);
    for (const auto encoding : {k::TileEncodings::TILE_ENCODING_JPEG, k::TileEncodings::TILE_ENCODING_UASTC,
                                k::TileEncodings::TILE_ENCODING_XUASTC, k::TileEncodings::TILE_ENCODING_IRIS})
        IFE_CHECK(static_cast<bool>(check(filler, TILE, encoding)));
    IFE_CHECK(!check(filler));
}

// ---- validate_file_structure ---------------------------------------------- //

/// A one-layer AV1 slide of two tiles and a NULL_TILE, written through the
/// Builder; tile 1 carries @p second.
std::vector<BYTE> av1_slide(const SequenceHeader& second, std::uint16_t length = TILE) {
    BuilderTileTableInfo grid;
    grid.encoding      = static_cast<Encoding>(k::TileEncodings::TILE_ENCODING_AV1);
    grid.format        = FORMAT_R8G8B8;
    grid.extent.width  = 3 * length;
    grid.extent.height = length;
    grid.extent.layers = {LayerExtent{.xTiles = 3, .yTiles = 1, .scale = 1.0f}};
    grid.tileLength    = length;
    const Builder builder = Builder::create({.capacity = Size{1} << 20});
    builder.set_tile_table(grid);
    SequenceHeader first;
    first.frame_width = first.frame_height = length;
    const std::vector<BYTE> zero = tile_stream(first);
    const std::vector<BYTE> one  = tile_stream(second);
    builder.append_tile(0, 0, zero.data(), zero.size());
    builder.append_tile(0, 1, one.data(), one.size());
    builder.append_null_tile(0, 2);
    const Offset tiles_at   = builder->append(b::TileOffsetsCreateInfo{.entries = builder.tile_offsets()});
    const Offset extents_at = builder->append(b::LayerExtentsCreateInfo{
        .entries = {{.X_TILES = 3, .Y_TILES = 1, .SCALE = 1.0f, .Z_PLANES = 0}}});
    const Offset table_at = builder->append(b::TileTableCreateInfo{
        .ENCODING             = k::TileEncodings::TILE_ENCODING_AV1,
        .FORMAT               = static_cast<k::PixelFormats>(grid.format),
        .TILE_OFFSETS_OFFSET  = tiles_at,
        .LAYER_EXTENTS_OFFSET = extents_at,
        .X_EXTENT             = grid.extent.width,
        .Y_EXTENT             = grid.extent.height,
        .TILE_LENGTH          = length});
    const Offset metadata_at = builder->append(b::MetadataCreateInfo{.MICRONS_PIXEL = 0.25f});
    builder.finalize({.tileTable = table_at, .metadata = metadata_at});
    return {builder->base(), builder->base() + builder.head()};
}

void test_validate_file_structure() {
    const std::vector<BYTE> clean = av1_slide({});
    IFE_CHECK(validate_file_structure({clean.data(), clean.size()}).succeeded());

    SequenceHeader bt709;
    bt709.matrix_coefficients = MC_BT_709;
    const std::vector<BYTE> wrong = av1_slide(bt709);
    const FileAccessInfo    at{wrong.data(), wrong.size()};
    IFE_CHECK(fails_with(validate_file_structure(at), FileError::CONFORMANCE, "Layer 0, tile 1"));
    IFE_CHECK(fails_with(validate_file_structure(at), FileError::CONFORMANCE, "matrix_coefficients 1"));
    // The read path is a lens over the bytes (D14) and never parses a tile
    // stream: a decoder refuses what it cannot convert.
    Abstraction::File file;
    IFE_CHECK(abstract_file_structure(at, file).succeeded());

    // The frame size is the slide's TILE_LENGTH, whatever it is.
    SequenceHeader small;   // 256 square, on a 512-pixel slide
    const std::vector<BYTE> mismatch = av1_slide(small, 512);
    IFE_CHECK(fails_with(validate_file_structure({mismatch.data(), mismatch.size()}),
                         FileError::CONFORMANCE, "max_frame_width_minus_1 255"));
    SequenceHeader large;
    large.frame_width = large.frame_height = 512;
    const std::vector<BYTE> matched = av1_slide(large, 512);
    IFE_CHECK(validate_file_structure({matched.data(), matched.size()}).succeeded());
}

}  // namespace

int main() {
    test_conformant_headers_pass();
    test_headers_an_encoder_wrote();
    test_each_rule_names_its_field();
    test_framing_and_bounds();
    test_truncation_at_every_byte();
    test_garbage_returns();
    test_other_encodings_are_not_parsed();
    test_validate_file_structure();
    if (g_failures == 0) std::printf("ife_av1_tile_stream_tests: PASS\n");
    return g_failures == 0 ? 0 : 1;
}

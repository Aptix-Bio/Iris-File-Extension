/**
 * @file ife_host_byte_order_tests.cpp
 * @brief Asserts the byte order of the host the test suite runs on.
 *
 * The big-endian CI job proves IFE's little-endian wire format on a
 * big-endian host only if its test binaries really execute as one. A
 * toolchain or emulator mistake (a native compiler picked up in place of the
 * cross compiler, or a test run without the emulator) would leave every other
 * suite green on a little-endian host and the job would prove nothing. This
 * test fails in that case.
 *
 * Usage: ife_host_byte_order_tests big|little
 *
 * Two witnesses, because they fail differently:
 *   - std::endian::native is what the compiler targeted;
 *   - the in-memory layout of a stored integer is what the executing CPU
 *     (or its emulator) did.
 */
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

int main(int argc, char** argv)
{
    if (argc != 2 || (std::string_view(argv[1]) != "big" &&
                      std::string_view(argv[1]) != "little")) {
        std::fprintf(stderr, "usage: %s big|little\n", argv[0]);
        return 2;
    }
    const bool want_big = std::string_view(argv[1]) == "big";

    const std::uint32_t value = 0x01020304u;
    unsigned char first = 0;
    std::memcpy(&first, &value, 1);
    // The most significant byte is stored first on a big-endian CPU.
    const bool memory_big = first == 0x01;
    const bool compiled_big = std::endian::native == std::endian::big;

    std::printf("expected %s; compiled for %s; memory layout %s\n",
                want_big ? "big" : "little",
                compiled_big ? "big" : "little",
                memory_big ? "big" : "little");

    int failures = 0;
    if (compiled_big != want_big) {
        std::fprintf(stderr, "FAIL: std::endian::native does not match\n");
        ++failures;
    }
    if (memory_big != want_big) {
        std::fprintf(stderr, "FAIL: the executing CPU's byte order does not match\n");
        ++failures;
    }
    return failures == 0 ? 0 : 1;
}

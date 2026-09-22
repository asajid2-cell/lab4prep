#include <cassert>   // assert
#include <cstdint>   // uint64_t, uint32_t
#include <iostream>  // std::cout

// Expands the binary representation of input by a factor of scale.
// e.g., expand(0b1111ull, 3) == 0b001001001001
uint64_t expand(uint64_t input, uint32_t scale) {
    assert(scale >= 1);

    uint64_t result = 0;
    // One bit of work per input bit, so at most 64 iterations: O(word size).
    for (uint32_t bit = 0; bit < 64; ++bit) {
        // Where this bit lands once every bit owns a block of `scale` bits.
        uint64_t shifted = static_cast<uint64_t>(bit) * scale;
        // Bits pushed past bit 63 fall off the top, and so does every later bit.
        if (shifted >= 64) {
            break;
        }
        if (input & (uint64_t{1} << bit)) {
            result |= (uint64_t{1} << shifted);
        }
    }
    return result;
}

int main() {
    // The two examples from the lab page.
    assert(expand(0b1111ull, 3) == 0b001001001001ull);
    assert(expand(0b0101ull, 2) == 0b00010001ull);

    // A scale of 1 leaves the value alone, and zero stays zero at any scale.
    assert(expand(0x0123456789abcdefull, 1) == 0x0123456789abcdefull);
    assert(expand(~0ull, 1) == ~0ull);
    assert(expand(0ull, 7) == 0ull);

    // Every single input bit must land exactly on bit (bit index * scale).
    // The loop condition stops at the bits that no longer fit in 64 bits.
    for (uint32_t scale = 1; scale <= 8; ++scale) {
        for (uint32_t bit = 0; static_cast<uint64_t>(bit) * scale < 64; ++bit) {
            assert(expand(uint64_t{1} << bit, scale) ==
                   (uint64_t{1} << (static_cast<uint64_t>(bit) * scale)));
        }
    }

    // Bits pushed past bit 63 are dropped, even partially.
    assert(expand(uint64_t{1} << 32, 2) == 0ull);  // bit 64, gone
    assert(expand(uint64_t{1} << 63, 3) == 0ull);  // bit 189, gone
    assert(expand(~0ull, 64) == 1ull);             // bit 0 survives, all else gone

    // assert() is compiled out in release builds, where this says nothing about the run.
#ifdef NDEBUG
    std::cout << "assertions disabled in this build (NDEBUG)\n";
#else
    std::cout << "all assertions passed\n";
#endif
    return 0;
}

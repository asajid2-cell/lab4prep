#include <cstddef>   // size_t
#include <cstdint>   // uint64_t
#include <iostream>  // std::cout
#include <list>      // std::list
#include <random>    // std::mt19937_64
#include <vector>    // std::vector

#include "timer.h"

constexpr size_t SIZE = 16000000;
constexpr uint64_t SEED = 0;

// Fills container with SIZE random values, re-seeding the generator first so that
// every container ends up holding exactly the same sequence.
template <typename Container>
void Fill(Container& container, Timer& timer, std::mt19937_64& gen, const char* label) {
    gen.seed(SEED);
    timer.restart();
    for (size_t i = 0; i < SIZE; ++i) {
        container.push_back(gen());
    }
    std::cout << label << ": " << timer.click<Timer::Micros>() << " us\n";
}

// Walks the container in order, adding every element into a uint64_t.
// Overflow is expected: we only care how long the walk takes.
template <typename Container>
void Sum(const Container& container, Timer& timer, const char* label) {
    timer.restart();
    uint64_t sum = 0;
    for (uint64_t value : container) {
        sum += value;
    }
    std::cout << label << ": sum = " << sum << ", " << timer.click<Timer::Micros>() << " us\n";
}

int main() {
    Timer timer;
    std::mt19937_64 gen(SEED);

    std::list<uint64_t> list;
    Fill(list, timer, gen, "list push_back");

    std::vector<uint64_t> vectorNoReserve;
    Fill(vectorNoReserve, timer, gen, "vector push_back (no reserve)");

    std::vector<uint64_t> vectorReserve;
    vectorReserve.reserve(SIZE);
    Fill(vectorReserve, timer, gen, "vector push_back (reserve)");

    // Iterate back over the containers in the order they were filled.
    Sum(list, timer, "list sum");
    Sum(vectorNoReserve, timer, "vector sum (no reserve)");

    return 0;
}

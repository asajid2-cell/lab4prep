# CMPUT 350 Lab 4 Prep

[Instructions](https://uofa-cmput350.github.io/materials/lab/4/prep.html)

## What is here

- `src/timer.h` - the `Timer` class. Constructed running; `restart()` starts the interval over;
  `click<T>()` returns the elapsed units of `T` since the last click/restart/construction and
  starts a new interval; `glance<T>()` is the same measurement without resetting. `Nanos`,
  `Micros`, `Millis`, `Seconds`, `Minutes` and `Hours` alias the `<chrono>` durations.
- `src/list_vs_array.cpp` - fills a `std::list`, a `std::vector` with no `reserve`, and a
  `std::vector` with `reserve`, with 16,000,000 values from `std::mt19937_64`. The generator is
  re-seeded to 0 before each fill so all three hold the same sequence, then each container is
  walked in turn and summed into a `uint64_t`.
- `src/expand.cpp` - `expand(input, scale)` spreads each input bit over a block of `scale` bits
  (`bit i` -> `bit i * scale`), dropping anything pushed past bit 63. Preconditions are asserted,
  and `main()` checks the two examples from the lab page, the `scale == 1` identity, every single
  bit at scales 1..8, and the truncation cases.

## Benchmark results

Numbers below were measured on the machine used for the prep, with `g++ 16.1` (MinGW-w64) on
Windows, once per build with the containers in the order shown. Run `cmake --build debug` and
`cmake --build release` and substitute your own - the absolute numbers depend heavily on the CPU
and its cache, the interesting part is the shape.

| case | debug (`-O0`) | release (`-O2`) |
| --- | --- | --- |
| `list` `push_back` | 957,518 us | 486,526 us |
| `vector` `push_back`, no `reserve` | 343,484 us | 91,982 us |
| `vector` `push_back`, with `reserve` | 298,636 us | 50,660 us |
| `list` sum | 92,537 us | 77,858 us |
| `vector` sum | 23,882 us | 7,488 us |

Both sums came out as `347898375840389362`, which is the check that both containers really do hold
the same values.

## Answers

### Timer: what if the unit were an `enum` parameter instead of a template type?

A template makes the unit a *type*, so `duration_cast<T>` is resolved when the function is
instantiated and each unit gets its own tiny function. An `enum` makes the unit a *value*, so one
function has to cope with every unit at runtime, and `duration_cast` cannot be used directly
because it needs a type at compile time. The usual shape would be to convert once and then pick:

```cpp
uint64_t click(TimeUnit unit) {
    auto now = std::chrono::steady_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - mLast).count();
    mLast = now;
    switch (unit) {
        case TimeUnit::NANOS:  return static_cast<uint64_t>(nanos);
        case TimeUnit::MICROS: return static_cast<uint64_t>(nanos / 1000);
        // ... one case per unit
    }
}
```

- **Implementation.** One function with a switch, or a table of per-unit divisors, instead of N
  instantiations. The cost is that the return type becomes uniform: you give up the ability to
  hand back a different width per unit, and units that are not a clean integer multiple of one
  another get awkward, since you are back to dividing an integer.
- **Maintainability.** Adding a unit means one enum value and one `case`, all in one place, which
  is neat, but nothing checks that you remembered the `case` unless you arrange for a warning on a
  non-exhaustive switch. Callers also cannot invent a unit. The template version documents its
  units at the call site (`Timer::Micros`) and fails to compile on anything that is not a
  `std::chrono::duration`, so it is self-checking, at the cost of exposing the whole chrono type
  zoo and producing template errors when someone gets it wrong.
- **Efficiency.** The `enum` version is worse per call: a runtime branch plus an integer division
  (or a scaling by a double), where the template version folds the conversion into the constant at
  compile time and inlines away to nothing. The one thing the `enum` buys is runtime flexibility -
  if the unit arrived from a command line flag or a config file, only the `enum` version can express
  that without a switch somewhere anyway.

So: template is zero-cost and compile-time checked; `enum` is one function, slightly cheaper to
read, and the only option if the unit is not known until runtime.

### How do the results for each container compare, and why?

**Filling.** The list is the expensive one. Every `push_back` allocates a brand new node (two
pointers plus the value, so ~24 bytes before the allocator's own overhead), which means 16,000,000
separate allocations, and each node lands wherever the allocator finds room. The vector grows one
contiguous buffer and doubles it when it fills, so it makes roughly 24 allocations in total and
moves its contents at `memcpy` speed. In release that is 486 ms against 92 ms.

**Walking.** The vector wins by much more here than it did on insertion, and it pulls further ahead
in release. Summing the vector is a sequential read: the hardware prefetcher recognises the
pattern, and each 64-byte cache line delivers eight `uint64_t`s, so the cost is closer to one
memory transaction per eight elements. The list has to load the next pointer *out of the current
node* before it knows where to go next, which is a dependent load chain the prefetcher cannot run
ahead of, and once the nodes no longer fit in cache every element is a stall. The payload is the
same 128 MB both times, but the list is really touching ~384 MB of scattered node memory. Release:
78 ms for the list against 7.5 ms for the vector, about 128 MB in 7.5 ms, or ~17 GB/s, which is
about what you would expect from a streaming read.

You can see the same thing in what the optimizer buys: going debug -> release speeds the vector sum
up by 3.2x but the list sum by only 1.2x. There is a lot of arithmetic and indirection to optimise
in the vector case and very little in a chain of dependent loads.

### Reserving vs not reserving the vector

Without `reserve`, the vector regrows whenever it is full: it allocates a larger buffer and moves
everything across. Doubling from 1 to 16,000,000 is about 24 reallocations, and the total movement
is roughly 1 + 2 + 4 + ... + 8M, so about 16,000,000 elements of extra copying, as much as the data
itself. Each regrowth also holds the old and the new buffer at the same time (up to about 192 MB
peak rather than 128 MB) and keeps asking the allocator for larger and larger blocks. With
`reserve(SIZE)` there is exactly one 128 MB allocation and no regrowth at all: 91,982 us against
50,660 us in release, so about 1.8x. In debug the same comparison is only 1.15x, because at `-O0`
the per-element bookkeeping (out-of-line `push_back`, no inlining) dominates the run and hides the
reallocation cost.

**If the element type had special copy semantics and no move semantics**, regrowth stops being a
`memcpy` and becomes a real copy-construction of every element. `std::vector` only takes the fast
move path when the move constructor exists and is `noexcept`; otherwise the strong exception
guarantee forces it to copy, because it must be able to roll back to the old buffer if a copy
throws (this is why lab 3 cared about `is_nothrow_move_constructible`). So the ~16,000,000 extra
copies stop being cheap bytes and become 16,000,000 calls to a copy constructor that does real
work, possibly allocating its own storage. The count of copies does not change, but their price
does, and `reserve` starts reducing a genuinely large cost rather than a memcpy: it goes from being
an optimisation to being essentially required. Peak memory during regrowth is worse too, since both
buffers then hold full deep copies.

### Why 16,000,000, and what would change with much smaller containers?

16,000,000 `uint64_t`s is 16,000,000 x 8 = 128,000,000 bytes, so about 128 MB against a stated L3
of 192 MB. That is the point: the contiguous array is deliberately sized to *just* fit in cache
(about two thirds of it), while the linked list cannot. A node is roughly 24 bytes before allocator
rounding, so the list's nodes are about 384 MB spread over many small allocations, well past the
cache - roughly three times the payload, and scattered rather than sequential.

Under LRU that difference decides everything. The array is walked in exactly the order it is laid
out, so the lines being pulled in are the lines about to be used and nothing live gets evicted
unnecessarily. The list's nodes are in allocation order, which has no relation to the walk order
once the allocator has handed out 16M small blocks, so each step effectively touches a
near-random address: it is a miss, and it evicts something you may have wanted. The array version
is streaming, the list version is pointer chasing, and the benchmark is arranged so that only the
array can be served from cache.

With much smaller containers - say 1,000,000 elements, 8 MB - both would sit comfortably in L3, and
partly in L2. The list would still be slower, because it still pays a pointer indirection and
touches ~3x the memory, but every one of those accesses would now be answered at cache latency
instead of from DRAM. The gap would shrink substantially, and what you would be measuring would be
instruction count and dependent-load latency rather than cache misses. The timer's own overhead
would also be a larger fraction of a much shorter run. The cache-layout lesson only becomes visible
once the working set crosses the cache size, which is exactly what 16,000,000 is chosen to
guarantee.

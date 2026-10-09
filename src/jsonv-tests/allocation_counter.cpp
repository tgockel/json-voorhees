#include "allocation_counter.hpp"

#if JSONV_TEST_COUNTS_ALLOCATIONS

#include <cstdlib>
#include <new>

namespace
{

/// Not atomic on purpose. Nothing in the suite starts a thread, and this increments on every allocation the test
/// binary makes -- including the benchmark timing loops, whose numbers are recorded in `.agents/perf-baseline.txt`.
/// A relaxed `std::atomic` increment is still a locked read-modify-write; a plain one is a store.
///
/// Constant-initialized, so it is already zero when the first allocation of process startup runs through here.
std::size_t allocation_count = 0U;

/// How many more allocations of at least `fail_min_size` bytes go through before the one which is to fail, counting
/// that one, or zero when none is armed.
std::size_t fail_countdown = 0U;
std::size_t fail_min_size  = 0U;

/// How many of the blocks `counted_allocate` has handed out have not come back through `counted_deallocate`. Every
/// pointer the replacement `operator delete`s are given came from a replacement `operator new`, since the over-aligned
/// forms stay paired with the implementation's own, so a block which never comes back is a leak -- unless the standard
/// library freed it some other way, which `live_allocations` warns of. Plain for the same reason `allocation_count` is.
std::size_t live_count = 0U;

void* counted_allocate(std::size_t size) noexcept
{
    ++allocation_count;
    if (fail_countdown != 0U && size >= fail_min_size && --fail_countdown == 0U)
        return nullptr;

    // Two live objects must never share an address, so a zero-sized request still has to get somewhere distinct.
    void* out = std::malloc(size ? size : 1U);
    if (out)
        ++live_count;
    return out;
}

void counted_deallocate(void* ptr) noexcept
{
    if (ptr)
        --live_count;
    std::free(ptr);
}

void* counted_allocate_or_throw(std::size_t size)
{
    if (void* out = counted_allocate(size))
        return out;
    else
        throw std::bad_alloc();
}

}

namespace jsonv_test
{

std::size_t total_allocations() noexcept
{
    return allocation_count;
}

std::size_t live_allocations() noexcept
{
    return live_count;
}

failing_allocation::failing_allocation(std::size_t nth, std::size_t min_size) noexcept
{
    fail_countdown = nth;
    fail_min_size  = min_size;
}

failing_allocation::~failing_allocation() noexcept
{
    fail_countdown = 0U;
}

}

/// Keeps link-time optimization from internalizing a replacement. A shared `libjsonv` can only reach one through the
/// test binary's exported symbol -- on macOS by coalescing with libc++abi's weak definition -- and AppleClang 17's LTO
/// link exported `operator new` and `operator new[]` but none of the others. Every block the library freed then went
/// to libc++abi's `operator delete` instead, and `live_allocations` counted it as never freed.
#if defined(__GNUC__)
#   define JSONV_TEST_REPLACEMENT __attribute__((used))
#else
#   define JSONV_TEST_REPLACEMENT
#endif

JSONV_TEST_REPLACEMENT
void* operator new(std::size_t size)                                   { return counted_allocate_or_throw(size); }
JSONV_TEST_REPLACEMENT
void* operator new[](std::size_t size)                                 { return counted_allocate_or_throw(size); }
JSONV_TEST_REPLACEMENT
void* operator new(std::size_t size, const std::nothrow_t&) noexcept   { return counted_allocate(size); }
JSONV_TEST_REPLACEMENT
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return counted_allocate(size); }

JSONV_TEST_REPLACEMENT
void operator delete(void* ptr) noexcept                               { counted_deallocate(ptr); }
JSONV_TEST_REPLACEMENT
void operator delete[](void* ptr) noexcept                             { counted_deallocate(ptr); }
JSONV_TEST_REPLACEMENT
void operator delete(void* ptr, std::size_t) noexcept                  { counted_deallocate(ptr); }
JSONV_TEST_REPLACEMENT
void operator delete[](void* ptr, std::size_t) noexcept                { counted_deallocate(ptr); }
JSONV_TEST_REPLACEMENT
void operator delete(void* ptr, const std::nothrow_t&) noexcept        { counted_deallocate(ptr); }
JSONV_TEST_REPLACEMENT
void operator delete[](void* ptr, const std::nothrow_t&) noexcept      { counted_deallocate(ptr); }

#endif

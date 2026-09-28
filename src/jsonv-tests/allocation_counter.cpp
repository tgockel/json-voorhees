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

/// The value `allocation_count` takes on the allocation which is to fail, or zero when none is armed -- which never
/// matches, since the count is at least one by the time it is compared.
std::size_t fail_at = 0U;

void* counted_allocate(std::size_t size) noexcept
{
    if (++allocation_count == fail_at)
    {
        fail_at = 0U;
        return nullptr;
    }

    // Two live objects must never share an address, so a zero-sized request still has to get somewhere distinct.
    return std::malloc(size ? size : 1U);
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

failing_allocation::failing_allocation(std::size_t nth) noexcept
{
    fail_at = allocation_count + nth;
}

failing_allocation::~failing_allocation() noexcept
{
    fail_at = 0U;
}

}

void* operator new(std::size_t size)                                   { return counted_allocate_or_throw(size); }
void* operator new[](std::size_t size)                                 { return counted_allocate_or_throw(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept   { return counted_allocate(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return counted_allocate(size); }

void operator delete(void* ptr) noexcept                               { std::free(ptr); }
void operator delete[](void* ptr) noexcept                             { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept                  { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept                { std::free(ptr); }
void operator delete(void* ptr, const std::nothrow_t&) noexcept        { std::free(ptr); }
void operator delete[](void* ptr, const std::nothrow_t&) noexcept      { std::free(ptr); }

#endif

/** \file
 *  Counting the heap allocations a region of code performs.
 *
 *  Copyright (c) 2026 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_TESTS_ALLOCATION_COUNTER_HPP_INCLUDED__
#define __JSONV_TESTS_ALLOCATION_COUNTER_HPP_INCLUDED__

#include <cstddef>

/** \def JSONV_TEST_COUNTS_ALLOCATIONS
 *  Is \c jsonv_test::allocation_counter available in this build?
 *
 *  Counting means replacing the global allocation functions, and AddressSanitizer provides its own -- taking them
 *  over would cost the whole suite ASan's `alloc-dealloc-mismatch` detection to gain two tests, which is the trade
 *  backwards. `detect_stack_use_after_return` moves stack frames onto the heap there as well, so the thing the
 *  counting tests assert is not even true under it. The counter is therefore not built under ASan and the tests
 *  which use it are compiled out rather than left to pass vacuously; the three plain CI configurations run them.
**/
#if defined(__SANITIZE_ADDRESS__)
#   define JSONV_TEST_COUNTS_ALLOCATIONS 0
#elif defined(__has_feature)
#   if __has_feature(address_sanitizer)
#       define JSONV_TEST_COUNTS_ALLOCATIONS 0
#   else
#       define JSONV_TEST_COUNTS_ALLOCATIONS 1
#   endif
#else
#   define JSONV_TEST_COUNTS_ALLOCATIONS 1
#endif

#if JSONV_TEST_COUNTS_ALLOCATIONS

namespace jsonv_test
{

/** The number of times a global \c operator \c new has run in this process.
 *
 *  Only the fundamental-alignment forms are counted. The over-aligned \c std::align_val_t overloads are deliberately
 *  left to the implementation, which keeps each of them paired with the \c operator \c delete the implementation
 *  chose for it; nothing these tests measure is over-aligned.
**/
std::size_t total_allocations() noexcept;

/** Counts the allocations performed while it is alive.
 *
 *  \code
 *  allocation_counter allocations;
 *  do_the_thing();
 *  const std::size_t cost = allocations.count();   // read it out *before* asserting: `ensure_eq` allocates
 *  ensure_eq(0U, cost);
 *  \endcode
**/
class allocation_counter
{
public:
    allocation_counter() noexcept :
            _mark(total_allocations())
    { }

    /// How many allocations have happened since this was created, or since the last \c reset.
    std::size_t count() const noexcept
    {
        return total_allocations() - _mark;
    }

    /// Start counting again from here.
    void reset() noexcept
    {
        _mark = total_allocations();
    }

private:
    std::size_t _mark;
};

}

#endif

#endif/*__JSONV_TESTS_ALLOCATION_COUNTER_HPP_INCLUDED__*/

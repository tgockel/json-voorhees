/// \file
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "node_arena.hpp"

#include <algorithm>

namespace jsonv::detail
{

// The header of a block is followed by memory aligned for anything. The first allocation from a block needs no padding,
// so it fits whenever the block was made big enough for it.
static constexpr std::size_t block_header_size =
    (sizeof(void*) + alignof(std::max_align_t) - 1U) / alignof(std::max_align_t) * alignof(std::max_align_t);

node_arena::node_arena(std::size_t first_block_size) noexcept :
        _last(nullptr),
        _next(nullptr),
        _end(nullptr),
        _next_block_size(std::max<std::size_t>(first_block_size, 256U))
{ }

node_arena::~node_arena() noexcept
{
    while (_last)
    {
        block_header* previous = _last->previous;
        ::operator delete(static_cast<void*>(_last));
        _last = previous;
    }
}

void* node_arena::allocate_from_new_block(std::size_t size)
{
    std::size_t capacity = std::max(_next_block_size, size);
    if (capacity > static_cast<std::size_t>(-1) - block_header_size)
        throw std::bad_alloc();

    auto* block = static_cast<std::byte*>(::operator new(block_header_size + capacity));
    _last = ::new (block) block_header{ _last };
    _next = block + block_header_size + size;
    _end  = block + block_header_size + capacity;

    _next_block_size = capacity <= static_cast<std::size_t>(-1) / 2U ? capacity * 2U : capacity;
    return block + block_header_size;
}

}

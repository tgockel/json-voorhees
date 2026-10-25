/// \file
/// Internal-only header for handing out short-lived nodes from a few large blocks.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

#include <cstddef>
#include <cstdint>
#include <new>

namespace jsonv::detail
{

/// Hands out memory from blocks it frees all at once when it goes away, so a container whose nodes all die together
/// costs a few allocations rather than one per node. Nothing is given back before then. Each block is twice the size of
/// the one before, so the number of blocks grows with the log of what is handed out.
///
/// It does what \c std::pmr::monotonic_buffer_resource does, which on macOS needs a deployment target of 14 or later.
class JSONV_LOCAL node_arena final
{
public:
    /// \param first_block_size How many bytes the first block holds. The first allocation makes it.
    explicit node_arena(std::size_t first_block_size) noexcept;

    node_arena(const node_arena&)            = delete;
    node_arena& operator=(const node_arena&) = delete;

    ~node_arena() noexcept;

    /// \a size bytes aligned to \a alignment, which is a power of two no greater than
    /// \c alignof(std::max_align_t).
    void* allocate(std::size_t size, std::size_t alignment)
    {
        std::size_t padding = static_cast<std::size_t>(-reinterpret_cast<std::uintptr_t>(_next)) & (alignment - 1U);
        if (static_cast<std::size_t>(_end - _next) < padding + size)
            return allocate_from_new_block(size);

        std::byte* out = _next + padding;
        _next = out + size;
        return out;
    }

private:
    struct block_header
    {
        block_header* previous;
    };

    void* allocate_from_new_block(std::size_t size);

private:
    block_header* _last;
    std::byte*    _next;
    std::byte*    _end;
    std::size_t   _next_block_size;
};

/// An allocator which takes its memory from a \c node_arena and never gives it back, for a node-based container which
/// is dropped whole.
template <typename T>
class JSONV_LOCAL arena_allocator
{
public:
    using value_type = T;

public:
    explicit arena_allocator(node_arena& arena) noexcept :
            _arena(&arena)
    { }

    template <typename U>
    arena_allocator(const arena_allocator<U>& other) noexcept :
            _arena(other._arena)
    { }

    T* allocate(std::size_t count)
    {
        if (count > static_cast<std::size_t>(-1) / sizeof(T))
            throw std::bad_array_new_length();
        return static_cast<T*>(_arena->allocate(count * sizeof(T), alignof(T)));
    }

    void deallocate(T*, std::size_t) noexcept
    { }

    template <typename U>
    bool operator==(const arena_allocator<U>& other) const noexcept
    {
        return _arena == other._arena;
    }

private:
    template <typename U>
    friend class arena_allocator;

    node_arena* _arena;
};

}

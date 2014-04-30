/** \file
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "../test.hpp"

#include <jsonv/detail/shared_buffer.hpp>

#include <algorithm>
#include <cstring>
#include <map>

using jsonv::detail::shared_buffer;
typedef shared_buffer::size_type size_type;

std::ostream& operator<<(std::ostream& os, const shared_buffer& self)
{
    return os << "shared_buffer[" << self.size() << "]";
}

TEST(shared_buffer_ctor_default)
{
    shared_buffer buff;
    
    ensure_eq(size_type(0), buff.size());
    ensure(buff.is_unique());
}

TEST(shared_buffer_ctor_sized)
{
    const size_type expected_size(10000);
    
    shared_buffer buff(expected_size);
    
    ensure_eq(expected_size, buff.size());
    ensure(buff.is_unique());
}

TEST(shared_buffer_test_ctor_sized_0)
{
    const size_type expected_size(0);
    
    shared_buffer buff(expected_size);
    
    ensure_eq(expected_size, buff.size());
    ensure(buff.is_unique());
}

TEST(shared_buffer_ctor_copy)
{
    const size_type expected_size(10000);
    
    shared_buffer buff(expected_size);
    
    ensure_eq(expected_size, buff.size());
    ensure(buff.is_unique());
    
    shared_buffer buff2(buff);
    
    ensure_eq(expected_size, buff2.size());
    ensure(!buff.is_unique());
    ensure(!buff2.is_unique());
    
    ensure_eq(buff, buff2);
}

TEST(shared_buffer_ctor_copy_unique_now)
{
    const size_type expected_size(10000);
    
    shared_buffer buff(expected_size);
    
    ensure_eq(expected_size, buff.size());
    ensure(buff.is_unique());
    
    shared_buffer buff2(buff, true);
    
    ensure_eq(expected_size, buff2.size());
    ensure(buff.is_unique());
    ensure(buff2.is_unique());
    
    ensure(buff.contents_equal(buff2));
}

TEST(shared_buffer_ctor_copy_and_change)
{
    const size_type expected_size(10000);
    
    shared_buffer buff(expected_size);
    
    ensure_eq(expected_size, buff.size());
    ensure(buff.is_unique());
    
    shared_buffer buff2(buff);
    
    ensure_eq(expected_size, buff2.size());
    ensure(!buff.is_unique());
    ensure(!buff2.is_unique());
    ensure_eq(buff, buff2);
    
    // Now, change buff2
    *buff2.get_mutable(0, 1) = 'p';
    
    ensure(buff.is_unique());
    ensure(buff2.is_unique());
    ensure_ne(buff, buff2);
    ensure(!buff.contents_equal(buff2));
}

TEST(shared_buffer_create_zero_filled)
{
    const size_type expected_size(10000);
    
    shared_buffer buff = shared_buffer::create_zero_filled(expected_size);
    
    ensure_eq(expected_size, buff.size());
    ensure(buff.is_unique());
    
    ensure(std::all_of(buff.cbegin(), buff.cend(), [] (char x) { return x == '\0'; }));
}

TEST(shared_buffer_evaluation_does_not_force_unique)
{
    const size_type expected_size(10000);
    
    shared_buffer buff(expected_size);
    
    ensure_eq(expected_size, buff.size());
    ensure(buff.is_unique());
    *buff.get_mutable(0, 0) = '!';
    
    shared_buffer buff2(buff);
    
    ensure(!buff.is_unique());
    ensure(!buff2.is_unique());
    ensure_eq(buff, buff2);
    
    ensure_eq('!', *buff.get(0));
    
    ensure(!buff.is_unique());
    ensure(!buff2.is_unique());
}

TEST(shared_buffer_slice_same)
{
    const size_type expected_size(10000);
    const size_type expected_slice_size = expected_size / 2;
    
    shared_buffer root(expected_size);
    shared_buffer slice1 = root.slice_until(expected_slice_size);
    shared_buffer slice2 = root.slice_until(expected_slice_size);
    
    ensure_eq(expected_slice_size, slice1.size());
    ensure_eq(slice1, slice2);
}

TEST(shared_buffer_test_slice_parent_gone)
{
    const size_type item_count = 1000;
    const char strip[] = "abcd";
    const size_type expected_size = sizeof(strip) * item_count;
    const size_type slice_item_count = item_count / 3;
    const size_type expected_slice_size = sizeof(strip) * slice_item_count;
    
    shared_buffer slice1, slice2;
    {
        shared_buffer root(expected_size);
        for (size_type i = 0; i < item_count; ++i)
            std::memcpy(root.get_mutable(i * sizeof(strip), sizeof(strip)), strip, sizeof strip);
        
        for (size_type i = 0; i < item_count; ++i)
            ensure_eq(0, std::memcmp(root.get(i * sizeof(strip), sizeof(strip)), strip, sizeof strip));
        
        slice1 = root.slice_until(expected_slice_size);
        slice2 = root.slice_until(expected_slice_size);
        
        ensure_eq(expected_slice_size, slice1.size());
        ensure_eq(slice1, slice2);
    }
    
    ensure_eq(expected_slice_size, slice1.size());
    ensure_eq(slice1, slice2);
    
    // make sure the data is still valid, too
    for (size_type i = 0; i < slice_item_count; ++i)
    {
        ensure_eq(0, std::memcmp(slice1.get(i * sizeof(strip), sizeof(strip)), strip, sizeof strip));
        ensure_eq(0, std::memcmp(slice2.get(i * sizeof(strip), sizeof(strip)), strip, sizeof strip));
    }
    
    ensure(!slice1.is_unique());
    ensure(!slice2.is_unique());
    
    slice2 = shared_buffer();
    
    ensure(slice1.is_unique());
}

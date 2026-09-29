/// \file
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"

#include <jsonv/detail/fixed_map.hpp>

#include <functional>

namespace jsonv_test
{

namespace
{

/// Where the keys a \c fixed_map may compare live: the one it was asked to find, and its own table.
struct key_bounds
{
    const char* sought  = nullptr;
    const char* begin   = nullptr;
    const char* end     = nullptr;
    bool        strayed = false;
};

/// \c std::less, noting in its \c key_bounds any key it is handed from anywhere else. Such a key was read from outside
/// the table, which AddressSanitizer does not see, since what lies past the table is the rest of the \c fixed_map.
struct bounds_checking_less
{
    key_bounds* bounds;

    bool operator()(const char& a, const char& b) const
    {
        // Nothing to check against while the constructor sorts the table
        if (bounds->begin)
        {
            for (const char* key : { &a, &b })
            {
                std::less<const char*> before;
                if (key != bounds->sought && (before(key, bounds->begin) || !before(key, bounds->end)))
                    bounds->strayed = true;
            }
        }
        return a < b;
    }
};

}

TEST(fixed_map_find_stays_in_its_table)
{
    key_bounds bounds;
    const jsonv::detail::fixed_map<char, char, 2, bounds_checking_less> map({ { 'a', '1' }, { 'c', '3' } },
                                                                             bounds_checking_less{ &bounds }
                                                                            );
    bounds.begin = reinterpret_cast<const char*>(map.begin());
    bounds.end   = reinterpret_cast<const char*>(map.end());

    // Before the first key, between them, on each, and after the last -- the one `lower_bound` answers with `end()`
    for (char key : { ' ', 'a', 'b', 'c', 'z' })
    {
        bounds.sought = &key;
        auto iter = map.find(key);
        ensure_eq(key == 'a' || key == 'c', iter != map.end());
        if (iter != map.end())
            ensure_eq(key, iter->first);
    }
    ensure(!bounds.strayed);
}

}

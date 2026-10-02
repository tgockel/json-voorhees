/** \file
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/algorithm.hpp>
#include <jsonv/functional.hpp>
#include <jsonv/value.hpp>

#include <cctype>
#include <string_view>

namespace jsonv
{

int compare(const value& a, const value& b)
{
    return compare(a, b, compare_traits());
}

struct compare_traits_icase :
        public compare_traits
{
    /** Compares strings a and b in a case-insensitive manner. It is not UTF-8 aware and I am not sure it needs to be.
     *  Taking views is what lets \c detail::compare_text_icase share it, so a string looked up by its text folds
     *  exactly as one held in a \c value does.
    **/
    static int compare_strings(std::string_view a, std::string_view b)
    {
        using std::begin;
        using std::end;
        
        auto aiter = begin(a);
        auto biter = begin(b);
        
        for ( ; aiter != end(a) && biter != end(b); ++aiter, ++biter)
        {
            // Through `unsigned char` for the same reason `match_string` does it: `std::tolower` is defined
            // only for `unsigned char` values and `EOF`, and a `char` holding a byte above 0x7f is negative.
            auto aa = std::tolower(static_cast<unsigned char>(*aiter));
            auto bb = std::tolower(static_cast<unsigned char>(*biter));
            if (aa == bb)
                continue;
            else if (aa < bb)
                return -1;
            else
                return 1;
        }
        
        return aiter == end(a) ? biter == end(b) ? 0 : -1
                               : 1;
    }
};

int compare_icase(const value& a, const value& b)
{
    return compare(a, b, compare_traits_icase());
}

int detail::compare_text(const value& a, std::string_view b)
{
    // `compare` asks the kinds first, and only two strings get as far as their text.
    if (int kinds = compare_traits::compare_kinds(a.kind(), kind::string))
        return kinds;
    else
        return a.as_string_view().compare(b);
}

int detail::compare_text_icase(const value& a, std::string_view b)
{
    if (int kinds = compare_traits_icase::compare_kinds(a.kind(), kind::string))
        return kinds;
    else
        return compare_traits_icase::compare_strings(a.as_string_view(), b);
}

}

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
#include "test.hpp"

#include <jsonv/algorithm.hpp>
#include <jsonv/functional.hpp>
#include <jsonv/value.hpp>
#include <jsonv/serialization_builder.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace jsonv_test
{

using namespace jsonv;

using string_list = std::vector<std::string>;

template <typename FJsonCmp, typename FStrCmp>
static void check_sort(const formats& fmts, string_list source, FJsonCmp json_cmp, FStrCmp str_cmp)
{
    value orig = to_json(source, fmts);
    if (source != deserialize<string_list>(orig, fmts))
        throw std::logic_error("Deserialization or encoding is broken");
    
    std::sort(source.begin(),     source.end(),     str_cmp);
    std::sort(orig.begin_array(), orig.end_array(), json_cmp);
    
    if (source != deserialize<string_list>(orig, fmts))
        throw std::logic_error("Sorting did not produce identical results");
}

/// Check \a json_cmp answers like \a str_cmp for every ordered pair from \a source. This includes each element paired
/// with itself, which is the only pair the strict and non-strict predicates disagree on.
template <typename FJsonCmp, typename FStrCmp>
static void check_agrees(const string_list& source, FJsonCmp json_cmp, FStrCmp str_cmp)
{
    for (const auto& a : source)
        for (const auto& b : source)
            ensure_eq(json_cmp(value(a), value(b)), str_cmp(a, b));
}

TEST(functional_sort_strings)
{
    formats fmts =
        formats::compose
        ({
            formats_builder()
                .register_container<std::vector<std::string>>(),
            formats::defaults()
        });
    std::vector<std::string> source = { "fire", "wind", "water", "earth", "heart" };
    
    check_sort(fmts, source, value_less(),    std::less<std::string>());
    check_sort(fmts, source, value_greater(), std::greater<std::string>());
}

TEST(functional_compare_strings)
{
    std::vector<std::string> source = { "fire", "wind", "water", "earth", "heart" };

    check_agrees(source, value_equal_to(),      std::equal_to<std::string>());
    check_agrees(source, value_not_equal_to(),  std::not_equal_to<std::string>());
    check_agrees(source, value_less(),          std::less<std::string>());
    check_agrees(source, value_less_equal(),    std::less_equal<std::string>());
    check_agrees(source, value_greater(),       std::greater<std::string>());
    check_agrees(source, value_greater_equal(), std::greater_equal<std::string>());
}

TEST(functional_text_comparison_agrees_with_the_value_ordering)
{
    // `enum_adapter` looks text up in a table ordered by `value_less` or `value_less_icase` without building a `value`
    // to hold it. That is only sound if the text sorts exactly where such a `value` would -- against every kind, not
    // only against other strings. Signs only: neither comparison promises a magnitude.
    const std::vector<value> values = { null,
                                        true,
                                        false,
                                        0,
                                        -1,
                                        2.5,
                                        std::nan(""),
                                        "",
                                        "a",
                                        "A",
                                        "b",
                                        "abc",
                                        "ABD",
                                        value(std::string("a\0b", 3U)),
                                        "\xC3\xA9",
                                        array(),
                                        object(),
                                      };
    const std::vector<std::string> texts = { "",
                                             "a",
                                             "A",
                                             "b",
                                             "abc",
                                             "abd",
                                             "ABC",
                                             std::string("a\0b", 3U),
                                             std::string("a\0", 2U),
                                             "\xC3\xA9",
                                             "\xC3\x89",
                                             "~",
                                           };

    auto sign = [] (int cmp) { return (cmp > 0) - (cmp < 0); };

    for (const auto& val : values)
    {
        for (const auto& text : texts)
        {
            ensure_eq(sign(compare(val, value(text))),       sign(detail::compare_text(val, text)));
            ensure_eq(sign(compare_icase(val, value(text))), sign(detail::compare_text_icase(val, text)));
        }
    }
}

}

/** \file
 *  
 *  Copyright (c) 2012-2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"

#include <jsonv/all.hpp>

#include <cstdint>
#include <cstring>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

TEST(move_to_self)
{
    const jsonv::value orig = jsonv::object({ {"a", 5} });
    jsonv::value x(orig);
    ensure_eq(orig, x);
    jsonv::value& same = x;
    x = std::move(same);
    ensure_eq(orig, x);
}

TEST(compare_bools)
{
    jsonv::value t1(true),
                 t2(true),
                 f1(false),
                 f2(false);
    
    ensure_eq(t1.compare(t1),  0);
    ensure_eq(t1.compare(t2),  0);
    ensure_eq(f1.compare(f1),  0);
    ensure_eq(f1.compare(f2),  0);
    ensure_eq(t1.compare(f1),  1);
    ensure_eq(f1.compare(t2), -1);
    
    ensure(t1 <= t2);
    ensure(t1 >= t2);
    ensure(t1 >  f1);
    ensure(t1 >= f1);
}

TEST(compare_arrs)
{
    jsonv::value a123  = jsonv::array({ 1, 2, 3 }),
                 a1234 = jsonv::array({ 1, 2, 3, 4 }),
                 b1234 = jsonv::array({ 1, 2, 3, 4 });
    
    ensure_eq(a1234.compare(b1234), 0);
    ensure_eq(a123.compare(a1234), -1);
    ensure_eq(a1234.compare(a123),  1);
}

TEST(value_equal_integer_decimal)
{
    ensure_eq(jsonv::value(2), jsonv::value(2.0));
    ensure_eq(jsonv::value(2.0), jsonv::value(2));
}

TEST(value_equal_float_decimal)
{
    ensure_eq(jsonv::value(2.5f), jsonv::value(2.5));
}

TEST(value_store_unordered_map)
{
    std::unordered_map<jsonv::value, std::int64_t> m;
    for (std::int64_t x = 0L; x < 1000; ++x)
    {
        auto ret = m.insert({ x, x });
        ensure(ret.second);
    }
    
    ensure_lt(1, m.bucket_count());
}

TEST(value_decimal_denorm_min_compares)
{
    // kind of a hack...we'll use 0.0 and *almost* 0.0
    union { std::uint64_t ival; double dval; } val;
    val.ival = 0x1;
    jsonv::value x = 0.0;
    jsonv::value y = val.dval;
    
    ensure_ne(x.as_decimal(), y.as_decimal());
    ensure_eq(x, y);
    ensure_eq(0, x.compare(y));
}

TEST(swap)
{
    jsonv::value x = jsonv::array({ 1, 2, 3 });
    jsonv::value y = "SOMETHING";
    swap(x, y);
    ensure_eq(jsonv::value("SOMETHING"), x);
    ensure_eq(jsonv::array({ 1, 2, 3 }), y);
}

TEST(swap_same)
{
    jsonv::value x = jsonv::array({ 1, 2, 3 });
    swap(x, x);
    ensure_eq(jsonv::array({ 1, 2, 3 }), x);
}

TEST(is_operations)
{
    jsonv::value num = 2.9;
    jsonv::value in_ = 5;
    jsonv::value arr = jsonv::array({ 1, 2, 3 });
    jsonv::value obj = jsonv::object({ {"arr", arr } });
    jsonv::value str = "SOMETHING";
    jsonv::value bol = true;
    jsonv::value nul = jsonv::null;
    
    ensure(num.is_decimal());
    ensure(in_.is_integer());
    ensure(in_.is_decimal());
    ensure(arr.is_array());
    ensure(obj.is_object());
    ensure(str.is_string());
    ensure(bol.is_boolean());
    ensure(nul.is_null());
}

TEST(hash_set_operations)
{
    jsonv::value num = 2.9;
    jsonv::value arr = jsonv::array({ 1, 2, 3 });
    jsonv::value obj = jsonv::object({ {"arr", arr } });
    jsonv::value str = "SOMETHING";
    jsonv::value bol = true;
    jsonv::value nul = jsonv::null;
    
    std::unordered_set<jsonv::value> set = { num, arr, obj, str, bol, nul };
    ensure_eq(6U, set.size());
    ensure_eq(1U, set.count(arr));
    set.erase(str);
    ensure_eq(0U, set.count(str));
    ensure_eq(5U, set.size());
}

/// Regression for the equality-reflexivity bug: a `value` whose `kind` byte has been corrupted
/// (e.g. via uninitialized memory or a wild write) must still satisfy `x == x`. Standard
/// containers and the library's own `std::hash<value>` rely on `EqualityComparable`, so a
/// non-reflexive `operator==` is a soundness hazard rather than a graceful degradation.
TEST(equality_is_reflexive_for_corrupt_kind)
{
    // Start from a default-constructed (null) value, which owns no heap resources. Scribbling
    // its `kind` byte therefore can't leak anything.
    jsonv::value v;

    unsigned char snapshot[sizeof(jsonv::value)];
    std::memcpy(snapshot, &v, sizeof v);

    unsigned char corrupted[sizeof(jsonv::value)];
    std::memcpy(corrupted, snapshot, sizeof v);
    // The 8-byte `_data` union sits at offset 0 (it's a default-constructed null, so all
    // its bytes are zero); the `_kind` byte lives somewhere in the trailing padding region.
    // Scribbling 0xFF over every byte past offset 8 forces `_kind` to an invalid value
    // without disturbing the (already-null) storage union.
    for (std::size_t i = 8; i < sizeof v; ++i)
        corrupted[i] = 0xFF;
    std::memcpy(&v, corrupted, sizeof v);

    // The kind is now invalid -- but reflexivity must hold.
    ensure(v == v);
    ensure(!(v != v));
    ensure_eq(0, v.compare(v));

    // Two *distinct* values where one is corrupt still compare unequal, and we must not
    // assert or throw doing so.
    jsonv::value good = 42;
    ensure(v != good);
    ensure(good != v);
    ensure(!(v == good));
    ensure(!(good == v));

    // Restore so the destructor sees a well-formed null.
    std::memcpy(&v, snapshot, sizeof v);
}

/// \file
/// Tests for \c jsonv::optional, which is \c std::optional except for a reference, where it stands in for C++26's
/// \c std::optional<T&> until every toolchain the library supports has it.
///
/// Copyright (c) 2016-2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"

#include <jsonv/optional.hpp>
#include <jsonv/value.hpp>

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace jsonv_test
{

using namespace jsonv;

namespace
{

/// An \c int reached only by an explicit conversion, which direct-initialization makes and copy-initialization does
/// not.
struct explicit_handle
{
    int* target;

    explicit operator int&() const noexcept
    {
        return *target;
    }
};

}

// Anything `std::optional` already takes is `std::optional`, so nothing which used one has to change.
static_assert(std::is_same_v<optional<int>, std::optional<int>>);
static_assert(std::is_same_v<optional<value>, std::optional<value>>);

// What `std::optional<T&>` promises about its representation: a pointer and nothing else, copied as one.
static_assert(sizeof(optional<const value&>) == sizeof(const value*));
static_assert(std::is_trivially_copyable_v<optional<const value&>>);

// Binding a temporary would leave it referring to an object gone by the end of the statement -- including one made by
// converting something to a `value` on the way in.
static_assert(!std::is_constructible_v<optional<const value&>, value&&>);
static_assert(!std::is_constructible_v<optional<const value&>, value>);
static_assert(!std::is_constructible_v<optional<const value&>, int>);
static_assert(!std::is_constructible_v<optional<value&>, const value&>);

// Adding `const` converts and taking it away does not, as for the references themselves.
static_assert(std::is_convertible_v<optional<value&>, optional<const value&>>);
static_assert(!std::is_convertible_v<optional<const value&>, optional<value&>>);

// What converts to a reference without making a temporary converts too, and implicitly where that conversion is.
static_assert(std::is_convertible_v<std::reference_wrapper<value>, optional<value&>>);
static_assert(std::is_convertible_v<std::reference_wrapper<value>, optional<const value&>>);
static_assert(std::is_convertible_v<std::reference_wrapper<const value>, optional<const value&>>);
static_assert(!std::is_constructible_v<optional<value&>, std::reference_wrapper<const value>>);
static_assert(std::is_convertible_v<std::reference_wrapper<int>, optional<int&>>);
static_assert(!std::is_constructible_v<optional<const int&>, double&>);
static_assert(std::is_constructible_v<optional<int&>, explicit_handle>);
static_assert(std::is_constructible_v<optional<const int&>, explicit_handle>);
static_assert(!std::is_convertible_v<explicit_handle, optional<int&>>);

TEST(optional_ref_empty)
{
    optional<const value&> x;
    ensure(!x);
    ensure(!x.has_value());
    ensure(x == std::nullopt);
    ensure_throws(std::bad_optional_access, x.value());
    ensure_eq(value(1), x.value_or(value(1)));

    optional<const value&> y = std::nullopt;
    ensure(!y);
}

TEST(optional_ref_refers_to_what_it_was_given)
{
    const value source = 1;

    optional<const value&> x = source;
    ensure(bool(x));
    ensure(x.has_value());
    ensure(x != std::nullopt);
    ensure(&*x == &source);
    ensure(&x.value() == &source);
    ensure(x.operator->() == &source);
    ensure_eq(kind::integer, x->kind());
    ensure_eq(source, x.value_or(value(2)));
}

TEST(optional_ref_assignment_rebinds)
{
    // Assigning goes to the `optional`, never through it to the object it refers to.
    value a = 1;
    value b = 2;

    optional<value&> x = a;
    x = b;
    ensure_eq(value(1), a);
    ensure(&*x == &b);

    x = std::nullopt;
    ensure(!x);
    ensure_eq(value(2), b);

    ensure(&x.emplace(a) == &a);
    ensure(&*x == &a);

    x.reset();
    ensure(!x);
    ensure_eq(value(1), a);
}

TEST(optional_ref_writes_through_a_mutable_reference)
{
    value a = 1;

    optional<value&> x = a;
    *x = 3;
    ensure_eq(value(3), a);
}

TEST(optional_ref_swap)
{
    const value a = 1;
    const value b = 2;

    optional<const value&> x = a;
    optional<const value&> y = b;
    swap(x, y);
    ensure(&*x == &b);
    ensure(&*y == &a);

    optional<const value&> empty;
    x.swap(empty);
    ensure(!x);
    ensure(&*empty == &b);
}

TEST(optional_ref_binds_through_a_reference_wrapper)
{
    // `std::ref` is the usual way to hand over a reference by value, and it has to work in every spelling a reference
    // does: initializing, assigning and emplacing.
    int a = 1;
    int b = 2;

    optional<int&> x = std::ref(a);
    ensure(&*x == &a);

    x = std::ref(b);
    ensure(&*x == &b);
    ensure_eq(1, a);

    ensure(&x.emplace(std::ref(a)) == &a);
    ensure(&*x == &a);

    const value source = 3;
    optional<const value&> y = std::cref(source);
    ensure(&*y == &source);
}

TEST(optional_ref_binds_through_an_explicit_conversion)
{
    // A reference is directly initialized from what it is given, which takes an explicit conversion. One which yields
    // an object already there is no different from an implicit one.
    int a = 1;
    int b = 2;

    optional<int&> x(explicit_handle{ &a });
    ensure(&*x == &a);

    ensure(&x.emplace(explicit_handle{ &b }) == &b);
    ensure(&*x == &b);

    optional<const int&> y(explicit_handle{ &a });
    ensure(&*y == &a);
}

TEST(optional_ref_adds_const)
{
    value a = 1;

    optional<value&>       x = a;
    optional<const value&> y = x;
    ensure(&*y == &a);

    optional<const value&> z = optional<value&>();
    ensure(!z);
}

}

/// \file
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"

#include <jsonv/demangle.hpp>
#include <jsonv/result.hpp>

#include <memory>

namespace jsonv_tests
{

using namespace jsonv;

static_assert(std::is_constructible_v<result<int>, int>,
              "A `result` should be constructible from `value_type` (if `value_type` != `error_type`)"
             );
static_assert(std::is_nothrow_constructible_v<result<int>, int>);
static_assert(std::is_nothrow_constructible_v<result<void>>);

TEST(result_ok)
{
    result<int> r = ok(10);
    ensure(r.is_ok());
    ensure_eq(10, r.value());
    ensure_eq(5, r.map([](int x) { return x / 2; }).value());
}

TEST(result_ok_widen_convert)
{
    result<int, int>   ri = ok(5);
    result<long, long> rl = ri;
    ensure(rl.is_ok());
    ensure_eq(5L, *rl);
}

TEST(result_error)
{
    result<int, std::string> r = error("Tacos");
    ensure(r.is_error());
    result<long, std::string> s = r.map([] (auto x) { return long(x); });
    ensure(s.is_error());
    ensure_eq(s.error(), "Tacos");
}

static_assert(!std::is_constructible_v<result<int, int>, int>,
              "A `result` with `value_type` == `error_type` should not be constructible from `value_type`"
             );

TEST(result_ok_to_same_type)
{
    result<int, int> r = ok(10);
    ensure(r.is_ok());
    ensure_eq(10, r.value());
}

TEST(result_void_map_to_int)
{
    result<void> r = ok{};
    result<int>  s = r.map([] { return 10; });
}

TEST(result_int_map_to_void)
{
    int found = 0;
    result<int>  r = ok(50);
    result<void> s = r.map([&](int x) { found = x; });
    ensure(s.is_ok());
    ensure_eq(50, found);
}

TEST(result_void_map_to_void)
{
    bool invoked = false;
    result<void> r = ok{};
    result<void> s = r.map([&] { invoked = true; });
    ensure(s.is_ok());
    ensure(invoked);
    s.value();
}

TEST(result_error_to_same_type)
{
    result<long, long> e = error(9);
    ensure(!e.is_ok());
    ensure(e.is_error());
    ensure_eq(9L, e.error());
}

TEST(result_move_only_map)
{
    result r = ok(std::make_unique<int>(10));
    result s = std::move(r).map([](std::unique_ptr<int> x) { return std::make_unique<long>(5 * *x); });
    ensure(r.state() == result_state::empty);
    ensure(s.is_ok());
    ensure_eq(50L, **s);
}

TEST(result_void_map_error_to_int)
{
    result<void, void> r = error{};
    result<void, int>  s = r.map_error([] { return 10; });
    ensure(s.is_error());
    ensure_eq(10, s.error());
}

TEST(result_int_map_error_to_void)
{
    int found = 0;
    result<int, int>  r = error(50);
    result<int, void> s = r.map_error([&](int x) { found = x; });
    ensure(s.is_error());
    ensure_eq(50, found);
}

TEST(result_void_map_error_to_void)
{
    bool invoked = false;
    result<void, void> r = error{};
    result<void, void> s = r.map_error([&] { invoked = true; });
    ensure(s.is_error());
    ensure(invoked);
    s.error();
}

TEST(result_move_only_map_error)
{
    result<std::unique_ptr<int>, std::unique_ptr<int>> r = error(std::make_unique<int>(10));
    result s = std::move(r).map_error([](std::unique_ptr<int> x) { return std::make_unique<long>(5 * *x); });
    ensure(r.state() == result_state::empty);
    ensure(s.is_error());
    ensure_eq(50L, *s.error());
}

TEST(result_ok_flat_map_to_value)
{
    result ro = ok(420);
    result rs = ro.map_flat([](int x) -> result<std::string> { return ok(std::to_string(x)); });
    ensure(rs.is_ok());
    ensure_eq(*rs, "420");
}

TEST(result_ok_flat_map_to_error)
{
    result<int, std::string> ro = ok(420);
    result rs = ro.map_flat([](int x) -> result<std::string, std::string> { return error(std::to_string(x)); });
    ensure(rs.is_error());
    ensure_eq(rs.error(), "420");
}

TEST(result_error_flat_map)
{
    result<int, int> ro = error(420);
    result rs = ro.map_flat([](int) -> result<std::string, int> { ensure(false); });
    ensure(rs.is_error());
    ensure_eq(rs.error(), 420);
}

TEST(result_ok_void_flat_map_to_value)
{
    result ro = ok{};
    result ri = ro.map_flat([]() -> result<int> { return ok(420); });
    ensure(ri.is_ok());
    ensure_eq(420, *ri);
}

TEST(result_ok_value_flat_map_to_void)
{
    result<int, int> ro = ok(420);
    result           rs = ro.map_flat([](int x) -> result<void, int> { return error(x); });
    ensure(rs.is_error());
    ensure_eq(420, rs.error());
}

TEST(result_ok_move_only_value_flat_map)
{
    result<std::unique_ptr<int>, std::unique_ptr<std::string>> ro = ok(std::make_unique<int>(105));
    result rs = std::move(ro)
            .map_flat([](std::unique_ptr<int> x) -> result<int, std::unique_ptr<std::string>> { return ok(*x); });
    ensure(rs.is_ok());
    ensure_eq(105, *rs);
    ensure(ro.state() == result_state::empty);
}

TEST(result_ok_recover_value)
{
    result<int> ro = ok(50);
    result<int> rs = ro.recover([](const std::exception_ptr&) -> optional<int> { ensure(false); return nullopt; });
    ensure(rs.is_ok());
    ensure_eq(50, *rs);
}

TEST(result_error_recover_value)
{
    result<int, int> ro = error(50);
    result<int, int> rs = ro.recover([](int x) -> optional<int> { return x; });
    ensure(rs.is_ok());
    ensure_eq(50, *rs);
}

TEST(result_error_do_not_recover_value)
{
    result<int, int> ro = error(50);
    result<int, int> rs = ro.recover([](int) -> optional<int> { return nullopt; });
    ensure(rs.is_error());
    ensure_eq(50, rs.error());
}

TEST(result_error_value_void_recover_value)
{
    result<int, void>  ro = error{};
    result<long, void> rs = ro.recover([]() -> optional<long> { return 420L; });
    ensure(rs.is_ok());
    ensure_eq(420L, *rs);
}

TEST(result_error_void_void_recover)
{
    result<void, void> ro = error{};
    result<void, void> rs = ro.recover([]() -> optional<ok<void>> { return ok{}; });
    ensure(rs.is_ok());
}

TEST(result_error_void_value_recover)
{
    result<void, int> ro = error(105);
    result<void, int> rs = ro.recover([](int) -> optional<ok<void>> { return ok{}; });
    ensure(rs.is_ok());
}

TEST(result_error_value_recover_flat)
{
    result<int, int> ro = error(210);
    result<int, int> ri = ro.recover_flat([](int x) -> result<int, int> { return ok(x * 2); });
    ensure(ri.is_ok());
    ensure_eq(420, *ri);
}

TEST(result_error_value_recover_flat_widening)
{
    result<std::string, int>  ro = error(210);
    result<std::string, long> rl = ro.recover_flat([](int x) -> result<std::string, long> { return error(x * 2); });
    ensure(rl.is_error());
    ensure_eq(420L, rl.error());
}

TEST(result_error_void_recover_flat)
{
    result<std::string, void> ro = error{};
    result<std::string, long> rl = ro.recover_flat([]() -> result<std::string, long> { return error(105L); });
    ensure(rl.is_error());
    ensure_eq(105L, rl.error());
}

TEST(result_error_void_recover_flat_to_void)
{
    result<void, long> ro = error(420L);
    result<void, void> rl = ro.recover_flat([](long x) -> result<void, void>
                                            {
                                                if (x == 420L)
                                                    return ok{};
                                                else
                                                    return error{};
                                            }
                                           );
    ensure(rl.is_ok());
}

TEST(result_error_move_only_recover_flat)
{
    result<std::unique_ptr<int>, std::unique_ptr<int>> ro = error(std::make_unique<int>(420));
    result<std::unique_ptr<int>, void>                 rs =
        std::move(ro).recover_flat([](std::unique_ptr<int> x) -> result<std::unique_ptr<int>, void>
                                   {
                                       return ok(std::move(x));
                                   }
                                  );
    ensure(ro.state() == result_state::empty);
    ensure(rs.is_ok());
    ensure_eq(420, **rs);
}

}

/// \file
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "allocation_counter.hpp"
#include "test.hpp"

#include <jsonv/ast.hpp>
#include <jsonv/demangle.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization/adapter_for.hpp>
#include <jsonv/serialization/extractor_construction.hpp>
#include <jsonv/serialization/function_extractor.hpp>
#include <jsonv/serialization/function_serializer.hpp>
#include <jsonv/value.hpp>
#include <jsonv/detail/scope_exit.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <new>
#include <ostream>
#include <string>
#include <tuple>
#include <typeinfo>
#include <typeindex>
#include <utility>

namespace jsonv_test
{

using namespace jsonv;

namespace
{

struct unassociated { };

struct my_thing
{
    int a;
    int b;
    std::string c;

    my_thing(const value& from, extraction_context& cxt) :
            a(cxt.extract<int>(from.at("a"))),
            b(cxt.extract<int>(from.at("b"))),
            c(cxt.extract<std::string>(from.at("c")))
    { }

    my_thing(int a, int b, std::string c) :
            a(a),
            b(b),
            c(std::move(c))
    { }

    static const extractor* get_extractor()
    {
        static extractor_construction<my_thing> instance;
        return &instance;
    }

    static const serializer* get_serializer()
    {
        static auto instance = make_serializer<my_thing>([] (const serialization_context& context, const my_thing& self)
            {
                return object({
                               { "a", context.to_json(self.a) },
                               { "b", context.to_json(self.b) },
                               { "c", context.to_json(self.c) },
                              }
                             );
            });
        return &instance;
    }

    bool operator==(const my_thing& other) const
    {
        return std::tie(a, b, c) == std::tie(other.a, other.b, other.c);
    }

    friend std::ostream& operator<<(std::ostream& os, const my_thing& self)
    {
        return os << "{ a=" << self.a << ", b=" << self.b << ", c=" << self.c << " }";
    }

    friend std::string to_string(const my_thing& self)
    {
        std::ostringstream os;
        os << self;
        return os.str();
    }
};

}

TEST(formats_equality)
{
    formats a;
    formats b = a;
    formats c = formats::compose({ a, b });

    ensure(a == b);
    ensure(a != c);
    ensure(b == a);
    ensure(c == c);
}

// Strange cases -- defaults, global and coerce must return sub-formats so nobody can change the real ones
TEST(formats_static_results_inequality)
{
    ensure(formats::defaults() != formats::defaults());
    ensure(formats::coerce()   != formats::coerce());
    ensure(formats::global()   != formats::global());
}

TEST(formats_throws_on_duplicate)
{
    formats fmt;
    fmt.register_extractor(my_thing::get_extractor());
    ensure_throws(std::invalid_argument, fmt.register_extractor(my_thing::get_extractor()));
    fmt.register_serializer(my_thing::get_serializer());
    ensure_throws(std::invalid_argument, fmt.register_serializer(my_thing::get_serializer()));
}

namespace
{

/// A type several registrations can serve at once, each telling which one it is.
struct tagged
{
    std::int64_t tag;
};

/// Extracts anything as a \c tagged carrying this adapter's tag and serializes any \c tagged as that tag, so which of
/// several registrations for \c tagged a lookup reached shows in what it produces.
class tagged_adapter final :
        public adapter_for<tagged>
{
public:
    explicit tagged_adapter(std::int64_t tag) :
            _tag(tag)
    { }

protected:
    std::expected<tagged, ast_node_type> create(extraction_context&, reader& from) const override
    {
        (void) from.next_value();
        return tagged{ _tag };
    }

    value to_json(const serialization_context&, const tagged&) const override
    {
        return value(_tag);
    }

private:
    std::int64_t _tag;
};

/// What a \c formats does with a \c tagged: the \c extractor and \c serializer a lookup finds and the tag each one
/// produces, with null and 0 where it finds none.
struct tagged_lookup
{
    const extractor*  ex         = nullptr;
    std::int64_t      extracted  = 0;
    const serializer* ser        = nullptr;
    std::int64_t      serialized = 0;

    explicit tagged_lookup(const formats& fmts)
    {
        try
        {
            ex        = &fmts.get_extractor(typeid(tagged));
            extracted = extract<tagged>(null, fmts).tag;
        }
        catch (const no_extractor&)
        { }

        try
        {
            ser        = &fmts.get_serializer(typeid(tagged));
            serialized = to_json(tagged{}, fmts).as_integer();
        }
        catch (const no_serializer&)
        { }
    }

    bool operator==(const tagged_lookup&) const = default;

    friend std::ostream& operator<<(std::ostream& os, const tagged_lookup& self)
    {
        return os << "extracted " << self.extracted << " by " << self.ex
                  << ", serialized " << self.serialized << " by " << self.ser;
    }
};

}

TEST(formats_refused_adapter_registration_takes_back_its_extractor)
{
    // An adapter registers its extractor before its serializer, so a serializer already there for the type is only
    // found once the extractor is in -- and refusing the registration then has to take that extractor back out.
    const auto existing = std::make_shared<tagged_adapter>(2);
    const auto incoming = std::make_shared<tagged_adapter>(3);

    formats fmts;
    fmts.register_serializer(existing);
    const tagged_lookup before(fmts);

    ensure_throws(duplicate_type_error, fmts.register_adapter(incoming.get()));
    ensure_eq(before, tagged_lookup(fmts));

    ensure_throws(duplicate_type_error, fmts.register_adapter(incoming));
    ensure_eq(before, tagged_lookup(fmts));
}

#if JSONV_TEST_COUNTS_ALLOCATIONS

namespace
{

/// One of the \c register_ overloads, applied to an adapter: which directions it registers, whether the \c formats
/// takes a share of what it registers, and the call itself.
struct registration
{
    bool extracts;
    bool serializes;
    bool shares;
    void (*perform)(formats&, const std::shared_ptr<const adapter>&, duplicate_type_action);
};

const registration registrations[] =
{
    { true,  false, false, [] (formats& fmts, const auto& adp, auto action)
                           { fmts.register_extractor(adp.get(), action); } },
    { true,  false, true,  [] (formats& fmts, const auto& adp, auto action)
                           { fmts.register_extractor(std::shared_ptr<const extractor>(adp), action); } },
    { false, true,  false, [] (formats& fmts, const auto& adp, auto action)
                           { fmts.register_serializer(adp.get(), action); } },
    { false, true,  true,  [] (formats& fmts, const auto& adp, auto action)
                           { fmts.register_serializer(std::shared_ptr<const serializer>(adp), action); } },
    { true,  true,  false, [] (formats& fmts, const auto& adp, auto action)
                           { fmts.register_adapter(adp.get(), action); } },
    { true,  true,  true,  [] (formats& fmts, const auto& adp, auto action)
                           { fmts.register_adapter(adp, action); } },
};

/// Registers a new \c tagged_adapter into a \c formats from \a prepare through each of \c registrations, with \c ignore
/// and with \c replace, failing each allocation the registration makes in turn until one goes through. Every attempt
/// gets a \c formats of its own, so \a prepare has to build one each time rather than hand out copies of one, which
/// would all share what it holds.
///
/// An attempt which fails has to leave the \c formats doing exactly what it did before, holding no share of the new
/// adapter. The one which goes through has to have done what its action says: a direction it registers answers with
/// the new adapter unless \c ignore found an entry there, which \a local_extractor and \a local_serializer tell. An
/// entry in a base does not count, since \c ignore only looks at the \c formats it is given.
template <typename FPrepare>
void check_failed_registrations(const FPrepare& prepare, bool local_extractor, bool local_serializer)
{
    const std::shared_ptr<const adapter> incoming = std::make_shared<tagged_adapter>(3);

    for (const registration& reg : registrations)
    {
        for (duplicate_type_action action : { duplicate_type_action::ignore, duplicate_type_action::replace })
        {
            for (std::size_t nth = 1U; ; ++nth)
            {
                formats             fmts = prepare();
                const tagged_lookup before(fmts);
                const long          shares = incoming.use_count();

                bool failed = false;
                {
                    // Nothing but the registration may allocate in here. That rules out `ensure_throws`, whose failure
                    // path allocates and so can catch the `std::bad_alloc` it caused itself and pass.
                    jsonv_test::failing_allocation fail(nth);
                    try
                    {
                        reg.perform(fmts, incoming, action);
                    }
                    catch (const std::bad_alloc&)
                    {
                        failed = true;
                    }
                }

                const tagged_lookup after(fmts);
                if (failed)
                {
                    ensure_eq(before, after);
                    ensure_eq(shares, incoming.use_count());
                    continue;
                }

                // Taking a share costs an allocation, so a registration which takes one and went through on the first
                // attempt never had a failure to survive.
                if (reg.shares)
                    ensure_gt(nth, 1U);

                const bool    replace  = action == duplicate_type_action::replace;
                tagged_lookup expected = before;
                if (reg.extracts && (replace || !local_extractor))
                {
                    expected.ex        = incoming.get();
                    expected.extracted = 3;
                }
                if (reg.serializes && (replace || !local_serializer))
                {
                    expected.ser        = incoming.get();
                    expected.serialized = 3;
                }
                ensure_eq(expected, after);
                break;
            }
        }
    }
}

}

TEST(formats_failed_registration_keeps_a_local_adapter)
{
    const auto existing = std::make_shared<tagged_adapter>(2);
    check_failed_registrations([&] { formats fmts; fmts.register_adapter(existing); return fmts; }, true, true);
}

TEST(formats_failed_registration_keeps_half_an_adapter)
{
    // Registering an adapter over half of one finds one entry and adds the other, so the entry it found is only safe if
    // failing to add the other leaves it alone.
    const auto existing = std::make_shared<tagged_adapter>(2);
    check_failed_registrations([&] { formats fmts; fmts.register_extractor(existing); return fmts; }, true, false);
    check_failed_registrations([&] { formats fmts; fmts.register_serializer(existing); return fmts; }, false, true);
}

TEST(formats_failed_registration_keeps_an_override_of_a_base)
{
    // Losing the override leaves no gap here for anything to notice: the base answers in its place.
    formats base;
    base.register_adapter(std::make_shared<tagged_adapter>(1));
    const auto existing = std::make_shared<tagged_adapter>(2);
    check_failed_registrations([&]
                               {
                                   formats fmts = formats::compose({ base });
                                   fmts.register_adapter(existing);
                                   return fmts;
                               },
                               true,
                               true
                              );
}

TEST(formats_failed_registration_takes_back_what_it_added)
{
    // With nothing for the type in the formats itself, every entry is one the registration added, so a failure has to
    // take all of them back out -- leaving nothing to find, or the base.
    formats base;
    base.register_adapter(std::make_shared<tagged_adapter>(1));
    check_failed_registrations([] { return formats(); }, false, false);
    check_failed_registrations([&] { return formats::compose({ base }); }, false, false);
}

#endif

TEST(extract_basics)
{
    value val = parse(R"({
                        "i": 5,
                        "d": 4.5,
                        "s": "thing",
                        "a": [ 1, 2, 3 ],
                        "o": { "i": 5, "d": 4.5 }
                      })");
    extraction_context cxt(formats::defaults());
    ensure(cxt.user_data() == nullptr);
    ensure_eq(val, cxt.extract<value>(val));
    ensure_eq(5, cxt.extract<std::int8_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint8_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int16_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint16_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int32_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint32_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int64_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint64_t>(val.at("i")));
    ensure_eq(4.5f, cxt.extract<float>(val.at("d")));
    ensure_eq(4.5, cxt.extract<double>(val.at("d")));
    ensure_eq("thing", cxt.extract<std::string>(val.at("s")));
    try
    {
        extraction_context::path_scope scope(cxt, "o");
        (void) cxt.extract<unassociated>(val.at("o"));
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& extract_err)
    {
        ensure_eq(path::create(".o"), extract_err.path());
        ensure(extract_err.nested_ptr());

        try
        {
            std::rethrow_exception(extract_err.nested_ptr());
        }
        catch (const no_extractor& noex)
        {
            ensure_eq(demangle(typeid(unassociated).name()), noex.type_name());
            ensure(noex.type_index() == std::type_index(typeid(unassociated)));
        }
    }
}

TEST(extract_object)
{
    formats fmts = formats::compose({ formats::defaults() });
    fmts.register_extractor(my_thing::get_extractor());

    my_thing res = extract<my_thing>(parse(R"({ "a": 1, "b": 2, "c": "thing" })"), fmts);
    to_string(res);
    ensure_eq(my_thing(1, 2, "thing"), res);
}

TEST(extract_object_with_unique_extractor)
{
    formats fmts = formats::compose({ formats::defaults() });
    fmts.register_extractor(std::unique_ptr<extractor>(new extractor_construction<my_thing>()));

    my_thing res = extract<my_thing>(parse(R"({ "a": 1, "b": 2, "c": "thing" })"), fmts);
    ensure_eq(my_thing(1, 2, "thing"), res);
}

TEST(extract_object_search)
{
    formats base_fmts;
    base_fmts.register_extractor(my_thing::get_extractor());
    formats fmts = formats::compose({ formats::defaults(), base_fmts });

    my_thing res = extract<my_thing>(parse(R"({ "a": 1, "b": 2, "c": "thing" })"), fmts);
    ensure_eq(my_thing(1, 2, "thing"), res);
}

TEST(extract_object_with_globals)
{
    {
        formats base_fmts;
        base_fmts.register_extractor(my_thing::get_extractor());
        formats::set_global(formats::compose({ formats::defaults(), base_fmts }));
    }
    auto reset_global_on_exit = jsonv::detail::on_scope_exit([] { formats::reset_global(); });

    my_thing res = extract<my_thing>(parse(R"({ "a": 1, "b": 2, "c": "thing" })"));
    ensure_eq(my_thing(1, 2, "thing"), res);
}

TEST(extract_coerce)
{
    value val = parse(R"({
                        "i": 5,
                        "d": 4.5,
                        "s": "10",
                        "a": [ 1, 2, 3 ],
                        "o": { "i": 5, "d": 4.5 }
                      })");
    extraction_context cxt(formats::coerce());

    // regular
    ensure_eq(val, cxt.extract<value>(val));
    ensure_eq(5, cxt.extract<std::int8_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint8_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int16_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint16_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int32_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint32_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int64_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint64_t>(val.at("i")));
    ensure_eq(4.5f, cxt.extract<float>(val.at("d")));
    ensure_eq(4.5, cxt.extract<double>(val.at("d")));
    ensure_eq("10", cxt.extract<std::string>(val.at("s")));

    // some coercing...
    ensure_eq("5", cxt.extract<std::string>(val.at("i")));
    ensure_eq(10, cxt.extract<int>(val.at("s")));
}

// Tests that even if we throw a completely bogus exception type, the extraction_context wraps it in an extraction_error
TEST(extractor_throws_random_thing)
{
    static auto instance = make_extractor([] (const value& from) -> unassociated { throw from; });
    formats locals;
    locals.register_extractor(&instance);

    value val = object({ { "a", 1 } });

    extraction_context cxt(locals);
    ensure_throws(extraction_error, cxt.extract<unassociated>(val));
    ensure_throws(extraction_error, cxt.extract<unassociated>(val.at("a")));
}

// For a non-const lvalue, a constructor taking a forwarding reference is a better match than the copy constructor. Left
// unconstrained, copying an adapter tried to build the wrapped function from the adapter and did not compile.
TEST(function_adapters_copy_from_non_const)
{
    auto extract_fn = [] (const value& from) { return from.as_integer() + 1; };
    function_extractor<std::int64_t, decltype(extract_fn)> extractor(extract_fn);
    function_extractor<std::int64_t, decltype(extract_fn)> extractor_copy(extractor);

    auto serialize_fn = [] (const serialization_context&, const std::int64_t& from) { return value(from + 1); };
    function_serializer<std::int64_t, decltype(serialize_fn)> serializer(serialize_fn);
    function_serializer<std::int64_t, decltype(serialize_fn)> serializer_copy(serializer);

    formats locals;
    locals.register_extractor(&extractor_copy);
    locals.register_serializer(&serializer_copy);

    ensure_eq(8, extraction_context(locals).extract<std::int64_t>(value(7)));
    ensure_eq(value(8), serialization_context(locals).to_json(std::int64_t(7)));
}

TEST(serialize_basics)
{
    serialization_context cxt(formats::defaults());
    ensure(cxt.user_data() == nullptr);
    ensure_eq(value(5), cxt.to_json(std::int8_t(5)));
    ensure_eq(value(5), cxt.to_json(std::uint8_t(5)));
    ensure_eq(value(5), cxt.to_json(std::int16_t(5)));
    ensure_eq(value(5), cxt.to_json(std::uint16_t(5)));
    ensure_eq(value(5), cxt.to_json(std::int32_t(5)));
    ensure_eq(value(5), cxt.to_json(std::uint32_t(5)));
    ensure_eq(value(5), cxt.to_json(std::int64_t(5)));
    ensure_eq(value(5), cxt.to_json(std::uint64_t(5)));
    ensure_eq(value(4.5), cxt.to_json(4.5));
    ensure_eq(value(4.5), cxt.to_json(4.5f));
    ensure_eq(value("thing"), cxt.to_json(std::string("thing")));

    try
    {
        (void) cxt.to_json(unassociated{});
    }
    catch (const no_serializer& noser)
    {
        ensure(noser.type_index() == std::type_index(typeid(unassociated)));
        ensure_eq(demangle(noser.type_name()), demangle(typeid(unassociated).name()));
    }
}

}

/// \file
/// Tests for the type-level write side: a \c serializer writing into a \c writer, the \c value bridges, the function
/// shapes and \c serialization_error.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "allocation_counter.hpp"
#include "test.hpp"

#include <jsonv/demangle.hpp>
#include <jsonv/path.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization/adapter_for.hpp>
#include <jsonv/serialization/container_adapter.hpp>
#include <jsonv/serialization/enum_adapter.hpp>
#include <jsonv/serialization/optional_adapter.hpp>
#include <jsonv/serialization/wrapper_adapter.hpp>
#include <jsonv/serialization/function_adapter.hpp>
#include <jsonv/serialization/function_serializer.hpp>
#include <jsonv/serialization/serializer_for.hpp>
#include <jsonv/value.hpp>
#include <jsonv/version.hpp>
#include <jsonv/writer.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <optional>
#include <streambuf>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace jsonv_test
{

using namespace jsonv;

static_assert(noexcept(std::declval<const serializer&>().get_type()),
              "serializer::get_type is noexcept, as deserializer::get_type is"
             );

namespace
{

/// A type nothing registers a serializer for.
struct unassociated
{ };

/// A type every serializer below writes the same way, so what each produces can be compared.
struct point
{
    std::int64_t x = 0;
    std::int64_t y = 0;
};

const std::string point_text = R"({"x":1,"y":2})";

void write_point(const point& from, writer& to)
{
    to.object_begin().key("x").integer(from.x).key("y").integer(from.y).object_end();
}

value point_value(const point& from)
{
    return object({ { "x", from.x }, { "y", from.y } });
}

/// A wrapper whose underlying structure is serialized at the same position.
struct point_wrapper
{
    using value_type = point;

    point held;

    explicit point_wrapper(point held = {}) : held(held) { }
    explicit operator point() const { return held; }
};

enum class mapped_kind
{
    structured,
    long_text,
    unmapped,
};

/// Writes token by token: the interface this change introduces.
class point_writer_serializer final :
        public serializer_for<point>
{
protected:
    void serialize(const serialization_context&, const point& from, writer& to) const override
    {
        write_point(from, to);
    }
};

/// Returns a \c value: the older interface, on the bridge.
class point_value_serializer final :
        public value_serializer_for<point>
{
protected:
    value to_json(const serialization_context&, const point& from) const override
    {
        return point_value(from);
    }
};

/// Both directions on the bridge.
class point_value_adapter final :
        public value_adapter_for<point>
{
protected:
    point create(deserialization_context&, const value& from) const override
    {
        return point{ from.at("x").as_integer(), from.at("y").as_integer() };
    }

    value to_json(const serialization_context&, const point& from) const override
    {
        return point_value(from);
    }
};

/// Written against the raw \c void* interface.
class point_raw_serializer final :
        public serializer
{
public:
    const std::type_info& get_type() const noexcept override
    {
        return typeid(point);
    }

    void serialize(const serialization_context&, const void* from, writer& to) const override
    {
        write_point(*static_cast<const point*>(from), to);
    }
};

/// Callable in two of the shapes \c make_serializer accepts, each producing something different.
struct dual_shape_serializer
{
    void operator()(const point&, writer& to) const
    {
        to.integer(1);
    }

    value operator()(const point&) const
    {
        return value(2);
    }
};

/// Writer-native composites, so a failure under them has a position to report.
struct inner
{
    unassociated b;
};

struct outer
{
    inner a;
};

class inner_serializer final :
        public serializer_for<inner>
{
protected:
    void serialize(const serialization_context& context, const inner& from, writer& to) const override
    {
        to.object_begin().key("b");
        context.serialize(from.b, to);
        to.object_end();
    }
};

class outer_serializer final :
        public serializer_for<outer>
{
protected:
    void serialize(const serialization_context& context, const outer& from, writer& to) const override
    {
        to.object_begin().key("a");
        context.serialize(from.a, to);
        to.object_end();
    }
};

/// A serializer for \c inner which throws whatever its \c thrower throws.
template <typename FThrow>
class throwing_inner_serializer final :
        public serializer_for<inner>
{
public:
    explicit throwing_inner_serializer(FThrow thrower) :
            _thrower(std::move(thrower))
    { }

protected:
    void serialize(const serialization_context&, const inner&, writer&) const override
    {
        _thrower();
    }

private:
    FThrow _thrower;
};

/// A \c formats holding only \a serializers, which must outlive it.
formats formats_with(std::initializer_list<const serializer*> serializers)
{
    formats fmts;
    for (const serializer* ser : serializers)
        fmts.register_serializer(ser);
    return fmts;
}

std::string serialize_to_text(const serialization_context& context, const point& from)
{
    std::ostringstream os;
    writer             to(os);
    context.serialize(from, to);
    return std::move(os).str();
}

/// Everything a serializer for \c point is expected to produce, through both entry points.
void ensure_point_serializes(const formats& fmts)
{
    const point           from{ 1, 2 };
    serialization_context context(fmts);
    ensure_eq(point_text, serialize_to_text(context, from));
    ensure_eq(point_value(from), context.to_json(from));
    ensure_eq(point_value(from), to_json(from, fmts));
}

}

TEST(serialize_writer_and_value_serializers_agree)
{
    point_writer_serializer writer_based;
    point_value_serializer  value_based;
    point_value_adapter     value_adapted;
    point_raw_serializer    raw;

    ensure_point_serializes(formats_with({ &writer_based }));
    ensure_point_serializes(formats_with({ &value_based }));
    ensure_point_serializes(formats_with({ &value_adapted }));
    ensure_point_serializes(formats_with({ &raw }));
}

TEST(serialize_function_shapes_agree)
{
    auto context_and_writer_fn = [] (const serialization_context&, const point& from, writer& to)
                                 {
                                     write_point(from, to);
                                 };
    auto writer_fn             = [] (const point& from, writer& to) { write_point(from, to); };
    auto context_fn            = [] (const serialization_context&, const point& from) { return point_value(from); };
    auto value_fn              = [] (const point& from) { return point_value(from); };
    auto deserialize_fn        = [] (const value& from)
                                 {
                                     return point{ from.at("x").as_integer(), from.at("y").as_integer() };
                                 };

    auto with_context_and_writer = make_serializer<point>(context_and_writer_fn);
    auto with_writer             = make_serializer<point>(writer_fn);
    auto with_context            = make_serializer<point>(context_fn);
    auto value_only              = make_serializer<point>(value_fn);
    auto adapted                 = make_adapter(deserialize_fn, writer_fn);

    ensure_point_serializes(formats_with({ &with_context_and_writer }));
    ensure_point_serializes(formats_with({ &with_writer }));
    ensure_point_serializes(formats_with({ &with_context }));
    ensure_point_serializes(formats_with({ &value_only }));
    ensure_point_serializes(formats_with({ &adapted }));
}

TEST(serialize_function_prefers_the_writer_shape)
{
    auto    ser  = make_serializer<point>(dual_shape_serializer());
    formats fmts = formats_with({ &ser });
    ensure_eq(value(1), to_json(point{}, fmts));
}

TEST(serialize_function_sees_the_version)
{
    auto    ser  = make_serializer<point>([] (const serialization_context& context, const point&, writer& to)
                                          {
                                              to.integer(context.version() ? std::int64_t(context.version()->major)
                                                                           : std::int64_t(-1));
                                          }
                                         );
    formats fmts = formats_with({ &ser });

    ensure_eq(value(3),  serialization_context(fmts, version(3, 1)).to_json(point{}));
    ensure_eq(value(-1), serialization_context(fmts).to_json(point{}));
}

TEST(serialize_by_value_builtins)
{
    // The built-in `char*` serializers take their argument by value, which the shape dispatch has to accept.
    const char* cstr = "x";
    ensure_eq(value("x"), to_json(cstr));

    char  buffer[]     = "y";
    char* mutable_cstr = buffer;
    ensure_eq(value("y"), to_json(mutable_cstr));
}

TEST(serialize_writes_one_value_where_the_writer_is)
{
    point_writer_serializer point_ser;
    formats                 fmts = formats::defaults();
    fmts.register_serializer(&point_ser);
    serialization_context context(fmts);

    std::ostringstream os;
    writer             to(os);
    to.array_begin();
    context.serialize(std::int64_t(1), to);
    context.serialize(std::string("x"), to);
    context.serialize(point{ 1, 2 }, to);
    to.array_end();

    const std::string text = std::move(os).str();
    ensure_eq(std::string(R"([1,"x",{"x":1,"y":2}])"), text);
}

TEST(serialize_simple_composites_preserve_values_and_placement)
{
    point_writer_serializer point_ser;
    container_adapter<std::vector<std::optional<point_wrapper>>> container;
    optional_adapter<std::optional<point_wrapper>> optional;
    wrapper_adapter<point_wrapper> wrapper;
    const value mapped = object({ { "points", array({ point_value(point{ 1, 2 }) }) } });
    enum_adapter<mapped_kind> enumeration("mapped_kind", { { mapped_kind::structured, mapped } });
    formats fmts = formats_with({ &point_ser, &container, &optional, &wrapper, &enumeration });
    serialization_context context(fmts);
    const std::vector<std::optional<point_wrapper>> from{ point_wrapper(point{ 1, 2 }), std::nullopt };
    const value expected = array({ point_value(point{ 1, 2 }), null });

    ensure_eq(expected, context.to_json(from));
    ensure_eq(mapped, context.to_json(mapped_kind::structured));
    ensure_eq(value(), context.to_json(mapped_kind::unmapped));
    ensure_eq(array(), context.to_json(decltype(from){}));

    std::ostringstream os;
    writer to(os);
    to.object_begin().key("items");
    context.serialize(from, to);
    to.key("mapped");
    context.serialize(mapped_kind::structured, to);
    to.key("missing");
    context.serialize(mapped_kind::unmapped, to);
    to.object_end();
    ensure_eq(std::string(R"({"items":[{"x":1,"y":2},null],"mapped":{"points":[{"x":1,"y":2}]},"missing":null})"),
              os.str());
}

TEST(serialize_simple_composites_keep_the_enclosing_error_path)
{
    inner_serializer inner_ser;
    container_adapter<std::vector<std::optional<inner>>> container;
    optional_adapter<std::optional<inner>> optional;
    formats fmts = formats_with({ &inner_ser, &container, &optional });
    std::ostringstream os;
    writer to(os);
    to.object_begin().key("items");
    try
    {
        serialization_context(fmts).serialize(std::vector<std::optional<inner>>{ std::nullopt, inner{} }, to);
        ensure(!"no_serializer was not thrown");
    }
    catch (const no_serializer& ex)
    {
        ensure_eq(path::create(".items[1].b"), ex.path());
        ensure(ex.type_index() == std::type_index(typeid(unassociated)));
        ensure(!ex.nested_ptr());
    }
    ensure_eq(std::string(R"({"items":[null,{"b":)"), os.str());
}

TEST(serialize_no_serializer_names_the_member_two_levels_down)
{
    outer_serializer      outer_ser;
    inner_serializer      inner_ser;
    formats               fmts = formats_with({ &outer_ser, &inner_ser });
    serialization_context context(fmts);
    const std::string     type_name = demangle(typeid(unassociated).name());

    std::ostringstream os;
    writer             to(os);
    try
    {
        context.serialize(outer{}, to);
        ensure(!"no_serializer was not thrown");
    }
    catch (const no_serializer& ex)
    {
        ensure_eq(path::create(".a.b"), ex.path());
        ensure(ex.type_index() == std::type_index(typeid(unassociated)));
        ensure_eq(type_name, std::string(ex.type_name()));
        ensure_eq(std::string("No serializer is registered"), ex.message());
        ensure(!ex.nested_ptr());
        ensure_eq("Serialization error at .a.b serializing " + type_name + ": No serializer is registered",
                  std::string(ex.what())
                 );
    }

    // It is a serialization_error, so a handler for those sees it as well.
    ensure_throws(serialization_error, to_json(outer{}, fmts));
}

TEST(serialize_error_is_rethrown_unchanged)
{
    outer_serializer          outer_ser;
    throwing_inner_serializer inner_ser([] {
                                            throw serialization_error(path::create(".custom"), typeid(int), "boom");
                                        });
    formats                   fmts = formats_with({ &outer_ser, &inner_ser });

    try
    {
        (void) to_json(outer{}, fmts);
        ensure(!"serialization_error was not thrown");
    }
    catch (const serialization_error& ex)
    {
        ensure_eq(path::create(".custom"), ex.path());
        ensure(ex.type_index() == std::type_index(typeid(int)));
        ensure_eq(std::string("boom"), ex.message());
        ensure(!ex.nested_ptr());
        ensure_eq("Serialization error at .custom serializing " + demangle(typeid(int).name()) + ": boom",
                  std::string(ex.what())
                 );
    }
}

TEST(serialize_container_no_serializer_names_the_element)
{
    container_adapter<std::vector<unassociated>> adapter;
    formats                                      fmts;
    fmts.register_adapter(&adapter);

    try
    {
        (void) to_json(std::vector<unassociated>(1), fmts);
        ensure(!"no_serializer was not thrown");
    }
    catch (const no_serializer& ex)
    {
        ensure(ex.type_index() == std::type_index(typeid(unassociated)));
        ensure_eq(path::create("[0]"), ex.path());
        ensure(!ex.nested_ptr());
    }
}

TEST(serialize_foreign_exception_is_wrapped_once)
{
    outer_serializer          outer_ser;
    throwing_inner_serializer inner_ser([] { throw 42; });
    formats                   fmts = formats_with({ &outer_ser, &inner_ser });

    try
    {
        (void) to_json(outer{}, fmts);
        ensure(!"serialization_error was not thrown");
    }
    catch (const serialization_error& ex)
    {
        ensure_eq(path::create(".a"), ex.path());
        ensure(ex.type_index() == std::type_index(typeid(inner)));
        // The name of the type comes from the C++ ABI, which not every platform exposes.
        ensure(ex.message().starts_with("Exception with type "));
        ensure(ex.nested_ptr());
        try
        {
            std::rethrow_exception(ex.nested_ptr());
        }
        catch (int cause)
        {
            ensure_eq(42, cause);
        }
    }
}

TEST(serialize_std_exception_is_wrapped_at_its_position)
{
    outer_serializer          outer_ser;
    throwing_inner_serializer inner_ser([] { throw std::runtime_error("boom"); });
    formats                   fmts = formats_with({ &outer_ser, &inner_ser });

    std::ostringstream os;
    writer             to(os);
    try
    {
        serialization_context(fmts).serialize(outer{}, to);
        ensure(!"serialization_error was not thrown");
    }
    catch (const serialization_error& ex)
    {
        ensure_eq(path::create(".a"), ex.path());
        ensure(ex.type_index() == std::type_index(typeid(inner)));
        ensure_eq(std::string("boom"), ex.message());
        ensure_eq("Serialization error at .a serializing " + demangle(typeid(inner).name()) + ": boom",
                  std::string(ex.what())
                 );
        ensure_throws(std::runtime_error, (std::rethrow_exception(ex.nested_ptr()), 0));
    }

    // The writer keeps the prefix of the document written before the failure.
    const std::string text = std::move(os).str();
    ensure_eq(std::string(R"({"a":)"), text);
}

TEST(serialize_no_serializer_at_the_root_has_no_position)
{
    const std::string type_name = demangle(typeid(unassociated).name());
    try
    {
        (void) to_json(unassociated{}, formats());
        ensure(!"no_serializer was not thrown");
    }
    catch (const no_serializer& ex)
    {
        ensure(ex.path().empty());
        ensure_eq("Serialization error serializing " + type_name + ": No serializer is registered",
                  std::string(ex.what())
                 );
    }
}

TEST(serialize_formats_serialize_names_where_the_value_was_wanted)
{
    formats               fmts;
    serialization_context context(fmts);
    const unassociated    from{};

    std::ostringstream os;
    writer             to(os);
    to.object_begin().key("k");
    try
    {
        fmts.serialize(typeid(unassociated), &from, to, context);
        ensure(!"no_serializer was not thrown");
    }
    catch (const no_serializer& ex)
    {
        ensure_eq(path::create(".k"), ex.path());
        ensure(ex.type_index() == std::type_index(typeid(unassociated)));
    }

    // A lookup which is not writing has no position to give.
    try
    {
        (void) fmts.get_serializer(typeid(unassociated));
        ensure(!"no_serializer was not thrown");
    }
    catch (const no_serializer& ex)
    {
        ensure(ex.path().empty());
        ensure(ex.type_index() == std::type_index(typeid(unassociated)));
    }
}

TEST(serialize_formats_serialize_does_not_wrap)
{
    throwing_inner_serializer inner_ser([] { throw std::runtime_error("raw"); });
    formats                   fmts = formats_with({ &inner_ser });
    serialization_context     context(fmts);
    const inner               from{};

    std::ostringstream os;
    writer             to(os);
    try
    {
        fmts.serialize(typeid(inner), &from, to, context);
        ensure(!"std::runtime_error was not thrown");
    }
    catch (const serialization_error&)
    {
        ensure(!"formats::serialize wrapped what the serializer threw");
    }
    catch (const std::runtime_error& ex)
    {
        ensure_eq(std::string("raw"), std::string(ex.what()));
    }
}

TEST(serialize_to_json_refuses_a_serializer_which_writes_nothing)
{
    auto silent = make_serializer<point>([] (const point&, writer&) { });
    auto open   = make_serializer<point>([] (const point&, writer& to) { to.array_begin(); });

    for (const serializer* ser : { static_cast<const serializer*>(&silent), static_cast<const serializer*>(&open) })
    {
        formats fmts = formats_with({ ser });
        try
        {
            (void) to_json(point{}, fmts);
            ensure(!"serialization_error was not thrown");
        }
        catch (const serialization_error& ex)
        {
            ensure(ex.type_index() == std::type_index(typeid(point)));
            ensure_throws(std::logic_error, (std::rethrow_exception(ex.nested_ptr()), 0));
        }
    }
}

#if JSONV_TEST_COUNTS_ALLOCATIONS

namespace
{

/// A container of itself, so a chain of them is as deep as it is long.
struct tree :
        std::vector<tree>
{ };

tree chain(std::size_t depth)
{
    tree out;
    if (depth > 0U)
        out.push_back(chain(depth - 1U));
    return out;
}

}

/// Discard text without allocating for an output buffer.
class discard_streambuf final :
        public std::streambuf
{
protected:
    std::streamsize xsputn(const char*, std::streamsize count) override { return count; }
    int_type overflow(int_type ch) override { return traits_type::not_eof(ch); }
};

TEST(serialize_simple_composites_to_text_allocate_only_for_frames)
{
    point_writer_serializer point_ser;
    container_adapter<std::vector<std::vector<std::optional<point_wrapper>>>> outer;
    container_adapter<std::vector<std::optional<point_wrapper>>> inner;
    optional_adapter<std::optional<point_wrapper>> optional;
    wrapper_adapter<point_wrapper> wrapper;
    const value mapped = object({ { "a-key-longer-than-the-small-string-buffer",
                                   array({ "a mapped string longer than the small-string buffer", 1 }) } });
    enum_adapter<mapped_kind> enumeration("mapped_kind", { { mapped_kind::structured, mapped },
                                                         { mapped_kind::long_text,
                                                           "a mapped string longer than the small-string buffer" } });
    formats fmts = formats_with({ &point_ser, &outer, &inner, &optional, &wrapper, &enumeration });
    serialization_context context(fmts);
    const std::vector<std::vector<std::optional<point_wrapper>>> from(
        32U, std::vector<std::optional<point_wrapper>>(32U, point_wrapper(point{ 1, 2 })));
    discard_streambuf buffer;
    std::ostream os(&buffer);
    writer to(os);
    auto write_document = [&]
                          {
                              to.array_begin();
                              context.serialize(from, to);
                              context.serialize(mapped_kind::structured, to);
                              context.serialize(mapped_kind::long_text, to);
                              context.serialize(mapped_kind::unmapped, to);
                              to.array_end();
                          };

    // Grow the writer's frame stack and key buffers once. Neither another element nor another mapped value
    // should build a tree, so the same document costs nothing on the second pass.
    write_document();
    allocation_counter allocations;
    write_document();
    const std::size_t cost = allocations.count();
    ensure_eq(std::size_t(0), cost);
}

TEST(serialize_bridge_moves_each_subtree_up)
{
    // A bridged composite builds a value for each of its parts through to_json and writes the result into the writer
    // it was handed. That write has to hand the tree over whole: walking it node by node into the parent's
    // value_encoder would rebuild every level of a nested container once per enclosing level, which is quadratic in
    // the depth. Linear means the second doubling of the depth costs what the first did.
    auto bridge = make_serializer<tree>([] (const serialization_context& context, const tree& from)
                                       {
                                           value out = array();
                                           for (const tree& child : from)
                                               out.push_back(context.to_json(child));
                                           return out;
                                       });
    formats fmts = formats_with({ &bridge });

    auto cost = [&](std::size_t depth)
                {
                    const tree         from = chain(depth);
                    allocation_counter allocations;
                    (void) to_json(from, fmts);
                    return allocations.count();
                };

    const std::size_t shallow         = cost(32U);
    const std::size_t middle          = cost(64U);
    const std::size_t deep            = cost(128U);
    const std::size_t first_doubling  = middle - shallow;
    const std::size_t second_doubling = deep - middle;
    ensure_lt(second_doubling, 3U * first_doubling);
}

#endif

}

/// \file
/// The built-in \c formats::defaults, \c formats::global and \c formats::coerce registries.
///
/// Every extractor here reads the AST node the \c reader is sitting on. These are the leaves of every extraction, so
/// this is where the \c value middleman stops being allocated: pulling an \c int out of JSON text now parses the token
/// and nothing else.
///
/// Copyright (c) 2015-2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/serialization/formats.hpp>
#include <jsonv/ast.hpp>
#include <jsonv/coerce.hpp>
#include <jsonv/demangle.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization/function_adapter.hpp>
#include <jsonv/serialization/function_extractor.hpp>
#include <jsonv/serialization/function_serializer.hpp>
#include <jsonv/value.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <sstream>
#include <string>
#include <optional>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "describe.hpp"

namespace jsonv
{

namespace
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reading a scalar                                                                                                   //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Turn an exception thrown by an \c ast_node accessor into an \c extraction_error::problem at \a from.
///
/// The nodes own the mechanism -- decoding an escape, parsing a number -- and know neither where in the document they
/// came from nor what the caller wanted. Translating here is what gives the failure a path.
JSONV_NODISCARD
std::unexpected<ast_node_type> problem_from_exception(extraction_context& context, reader& from)
{
    return context.problem(context.problem_path(from), std::current_exception());
}

/// Record that the node \a from is on is not one this extractor accepts, having \a wanted something else.
///
/// \c extraction_context::expect is the right tool wherever the list of acceptable node types is short enough to read
/// back in a message. The coercing extractors accept most of the tape, so they say what they wanted instead.
JSONV_NODISCARD
std::unexpected<ast_node_type> problem_wrong_type(extraction_context& context, reader& from, std::string_view wanted)
{
    auto               found = from.current().type();
    std::ostringstream os;

    os << "Read node of type " << describe(found) << " when expecting " << wanted;
    (void) context.problem(context.problem_path(from), std::move(os).str());

    // The error channel carries what was really there rather than the `error` sentinel, so a caller can branch on it.
    return std::unexpected(found);
}

/// Read the string the reader is on, decoding escape sequences if the source used any.
///
/// A \c string_canonical node is a view of the source text with no translation to do, which is the whole reason the
/// two node types exist; \c string_escaped has to be decoded.
JSONV_NODISCARD
std::expected<std::string, ast_node_type> read_string(extraction_context& context, reader& from)
{
    try
    {
        if (from.current().type() == ast_node_type::string_canonical)
            return std::string(from.current().as<ast_node::string_canonical>().value());
        else
            return from.current().as<ast_node::string_escaped>().value();
    }
    catch (...)
    {
        // `parse_index` validates the *syntax* of an escape without decoding it, so a well-formed `\uD800` with no
        // low surrogate to pair with is only caught here. That is a problem with the document at a place in it, not an
        // escaping exception for the caller of `extract` to deal with.
        return problem_from_exception(context, from);
    }
}

/// Check that the reader is on a number, recording a problem naming what was found if it is not.
///
/// This is \c extraction_context::expect over the two numeric node types, except where the reader is lending a
/// \c value. A value-backed reader renders a \c kind::decimal into a token with \c format_decimal, which has nowhere
/// to put a non-finite \c double, so such a value arrives as \c literal_null -- what encoding it writes, and not what
/// something reading the tree should conclude it holds. Where there is a \c value to ask, its \c kind decides and the
/// rendering does not.
JSONV_NODISCARD
std::expected<void, ast_node_type> expect_number(extraction_context& context, reader& from)
{
    if (const value* lent = from.current_value())
    {
        if (lent->kind() == jsonv::kind::decimal || lent->kind() == jsonv::kind::integer)
            return {};
    }

    return context.expect(from, { ast_node_type::decimal, ast_node_type::integer });
}

/// Read the number the reader is on as a \c double, having established with \ref expect_number that it is one.
///
/// An integer token is also a well-formed decimal token, so the decimal node's parser reads either. Going by way of
/// \c ast_node::integer::value() would round a magnitude beyond \c std::int64_t to that type's bound rather than to
/// the nearest \c double.
JSONV_NODISCARD
std::expected<double, ast_node_type> read_decimal(extraction_context& context, reader& from)
{
    // The `value` is the number; the token is a rendering of it. See `expect_number`.
    if (const value* lent = from.current_value())
        return lent->as_decimal();

    auto token = from.current().token_raw();
    try
    {
        return ast_node::decimal(token.data(), token.size()).value();
    }
    catch (...)
    {
        // A magnitude with no finite `double` to round to -- `1e400`. Underflow to zero is deliberate and does not
        // arrive here.
        return problem_from_exception(context, from);
    }
}

/// Read the integer token the reader is on as a \c T, which is where the built-ins' range policy lives.
///
/// \c std::from_chars reads the token against the destination type directly, so a literal too large for it is a
/// reported failure rather than the bound \c ast_node::integer::value() saturates to and the modular wrap a narrowing
/// conversion would then apply to that. A negative literal is likewise out of range for an unsigned \c T rather than
/// its two's-complement reinterpretation.
template <typename T>
JSONV_NODISCARD
std::expected<T, ast_node_type> read_integer(extraction_context& context, reader& from)
{
    auto token = from.current().token_raw();
    T    out{};

    auto result = std::from_chars(token.data(), token.data() + token.size(), out);
    if (result.ec == std::errc() && result.ptr == token.data() + token.size())
        return out;

    std::ostringstream os;
    os << "Integer " << token << " is out of range for " << demangle(typeid(T).name());
    return context.problem(context.problem_path(from), std::move(os).str());
}

/// Is the value the reader is on the empty one of its kind? This is all \c coerce_boolean needs to know about a
/// string, an object or an array, and none of the three has to be read to answer it: a string token carries its own
/// length and both structure openers carry their element count.
JSONV_NODISCARD
bool is_empty(const ast_node& node)
{
    if (node.type() == ast_node_type::object_begin)
        return node.as<ast_node::object_begin>().element_count() == 0U;
    else if (node.type() == ast_node_type::array_begin)
        return node.as<ast_node::array_begin>().element_count() == 0U;
    else
        // The token includes the surrounding quotations, and every escape sequence decodes to at least one character.
        return node.token_raw().size() <= 2U;
}

/// Is the integer token the reader is on a zero? JSON forbids a leading zero, so the only two spellings are `0` and
/// `-0` and nothing has to be parsed to rule them out.
JSONV_NODISCARD
bool is_zero_integer(const ast_node& node)
{
    auto token = node.token_raw();
    return token == "0" || token == "-0";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats::defaults                                                                                                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

JSONV_NODISCARD
std::expected<value, ast_node_type> extract_value(reader& from)
{
    // A value-backed reader is already holding the tree, so copy what it lends rather than rebuilding one token at a
    // time -- which would also drop what a `value` can hold and JSON cannot, such as a non-finite `kind::decimal`.
    if (const value* borrowed = from.current_value())
    {
        value out = *borrowed;
        (void) from.next_value();
        return out;
    }

    return read_value(from);
}

JSONV_NODISCARD
std::expected<std::string, ast_node_type> extract_string(extraction_context& context, reader& from)
{
    auto matched = context.expect(from, { ast_node_type::string_canonical, ast_node_type::string_escaped });
    if (!matched)
        return std::unexpected(matched.error());

    auto out = read_string(context, from);
    if (out)
        (void) from.next_token();

    return out;
}

JSONV_NODISCARD
std::expected<std::string_view, ast_node_type> extract_string_view(extraction_context& context, reader& from)
{
    auto matched = context.expect(from, { ast_node_type::string_canonical, ast_node_type::string_escaped });
    if (!matched)
        return std::unexpected(matched.error());

    std::string_view out;
    if (context.source_is_temporary())
    {
        // The source is storage the extraction owns -- a tree materialised for the occasion, or text handed over to
        // it -- so a view of it would name storage which is already gone by the time the caller has it.
        return context.problem(context.problem_path(from),
                               "Cannot extract a std::string_view from a source which is freed when extraction "
                               "finishes: the string it would refer to is owned by the extraction. Extract a "
                               "std::string instead."
                              );
    }
    else if (const value* borrowed = from.current_value())
    {
        // Value-backed: view the caller's own string rather than the token the reader synthesised for it, which lives
        // in the reader's arena and dies with the reader.
        out = borrowed->as_string_view();
    }
    else if (from.current().type() == ast_node_type::string_canonical)
    {
        // Text-backed and canonical: the token is the string, so this views the source document directly.
        out = from.current().as<ast_node::string_canonical>().value();
    }
    else
    {
        return context.problem(context.problem_path(from),
                               "Cannot extract a std::string_view from a JSON string written with escape sequences: "
                               "it has no decoded form in the source to refer to. Extract a std::string instead."
                              );
    }

    (void) from.next_token();
    return out;
}

JSONV_NODISCARD
std::expected<bool, ast_node_type> extract_boolean(extraction_context& context, reader& from)
{
    auto matched = context.expect(from, { ast_node_type::literal_true, ast_node_type::literal_false });
    if (!matched)
        return std::unexpected(matched.error());

    bool out = from.current().type() == ast_node_type::literal_true;
    (void) from.next_token();
    return out;
}

template <typename T>
JSONV_NODISCARD
std::expected<T, ast_node_type> extract_integer(extraction_context& context, reader& from)
{
    auto matched = context.expect(from, ast_node_type::integer);
    if (!matched)
        return std::unexpected(matched.error());

    auto out = read_integer<T>(context, from);
    if (out)
        (void) from.next_token();

    return out;
}

/// The default formats accept an \c integer where a \c double is wanted, which is what \c value::as_decimal has always
/// done.
JSONV_NODISCARD
std::expected<double, ast_node_type> extract_decimal(extraction_context& context, reader& from)
{
    auto matched = expect_number(context, from);
    if (!matched)
        return std::unexpected(matched.error());

    auto out = read_decimal(context, from);
    if (out)
        (void) from.next_token();

    return out;
}

JSONV_NODISCARD
std::expected<float, ast_node_type> extract_float(extraction_context& context, reader& from)
{
    auto out = extract_decimal(context, from);
    if (!out)
        return std::unexpected(out.error());

    return static_cast<float>(*out);
}

template <typename T>
void register_integer_adapter(formats& fmt, duplicate_type_action on_duplicate = duplicate_type_action::exception)
{
    static auto instance =
        make_adapter([] (extraction_context& context, reader& from) { return extract_integer<T>(context, from); },
                     [] (const T& from) { return value(static_cast<std::int64_t>(from)); }
                    );
    fmt.register_adapter(&instance, on_duplicate);
}

formats create_default_formats()
{
    formats fmt;

    static auto json_adapter = make_adapter(extract_value, [] (const value& from) { return from; });
    fmt.register_adapter(&json_adapter);

    static auto string_adapter = make_adapter(extract_string, [] (const std::string& from) { return value(from); });
    fmt.register_adapter(&string_adapter);

    // The only built-in which hands back a view of what it was given, and so the only one which has to care where that
    // storage came from.
    static auto string_view_adapter = make_adapter(extract_string_view,
                                                   [] (const std::string_view& from) { return value(from); }
                                                  );
    fmt.register_adapter(&string_view_adapter);

    static auto cchar_ptr_serializer = make_serializer<const char*>([] (const char* from) { return value(from); });
    fmt.register_serializer(&cchar_ptr_serializer);
    static auto char_ptr_serializer = make_serializer<char*>([] (char* from) { return value(from); });
    fmt.register_serializer(&char_ptr_serializer);

    static auto bool_adapter = make_adapter(extract_boolean, [] (const bool& from) { return value(from); });
    fmt.register_adapter(&bool_adapter);

    register_integer_adapter<std::int8_t>(fmt);
    register_integer_adapter<std::uint8_t>(fmt);
    register_integer_adapter<std::int16_t>(fmt);
    register_integer_adapter<std::uint16_t>(fmt);
    register_integer_adapter<std::int32_t>(fmt);
    register_integer_adapter<std::uint32_t>(fmt);
    register_integer_adapter<std::int64_t>(fmt);
    register_integer_adapter<std::uint64_t>(fmt);

    // These common types are usually covered by the explicitly-sized integers, but try to add them for platforms like
    // OSX and Windows.
    register_integer_adapter<std::size_t>(fmt, duplicate_type_action::ignore);
    register_integer_adapter<std::ptrdiff_t>(fmt, duplicate_type_action::ignore);
    register_integer_adapter<long>(fmt, duplicate_type_action::ignore);
    register_integer_adapter<unsigned long>(fmt, duplicate_type_action::ignore);

    static auto double_adapter = make_adapter(extract_decimal, [] (const double& from) { return value(from); });
    fmt.register_adapter(&double_adapter);
    static auto float_adapter = make_adapter(extract_float, [] (const float& from) { return value(from); });
    fmt.register_adapter(&float_adapter);

    return fmt;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats::coerce                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Run one of the \c coerce_X functions over \a source, turning the \c kind_error it raises for a kind it will not
/// read into a problem at \a from.
///
/// Those functions are the definition of what \c formats::coerce accepts, so the rules stay with them and this only
/// has to say where a failure was.
template <typename FCoerce>
JSONV_NODISCARD
auto coerce_checked(extraction_context& context, reader& from, const value& source, const FCoerce& coerce)
    -> std::expected<std::remove_cvref_t<decltype(coerce(source))>, ast_node_type>
{
    try
    {
        return coerce(source);
    }
    catch (...)
    {
        return problem_from_exception(context, from);
    }
}

/// Run one of the \c coerce_X functions over the string the reader is on, which is how a string reaches a number.
///
/// This is the one coercion which cannot be answered from the token: what the string says has to be read as JSON in
/// its own right, which is what \c coerce_integer and \c coerce_decimal have always done with it. Issue #193 is about
/// replacing that with a direct numeric scan, which changes what a string is allowed to say, so it is left to be
/// decided on its own.
template <typename FCoerce>
JSONV_NODISCARD
auto coerce_from_string(extraction_context& context, reader& from, const FCoerce& coerce)
    -> std::expected<std::remove_cvref_t<decltype(coerce(std::declval<const value&>()))>, ast_node_type>
{
    auto text = read_string(context, from);
    if (!text)
        return std::unexpected(text.error());

    return coerce_checked(context, from, value(*std::move(text)), coerce);
}

/// The subtree the reader is lending, which the coercing extractors answer from in preference to the tape.
///
/// \c coerce_string, \c coerce_boolean and friends are what these rules *are*, so handing them the tree they were
/// written against is both the most faithful answer and the cheapest one -- a composite is weighed or encoded where
/// it sits rather than rebuilt through \c read_value first. It is also the only way to see a non-finite
/// \c kind::decimal, which the reader renders as \c literal_null; see \ref expect_number.
///
/// \returns The coerced result and an advanced reader; or \c nullopt if this reader has no tree to lend, leaving
///          \a from untouched for the caller to read off the tape.
template <typename FCoerce>
JSONV_NODISCARD
auto coerce_lent_value(extraction_context& context, reader& from, const FCoerce& coerce)
    -> std::optional<std::expected<std::remove_cvref_t<decltype(coerce(std::declval<const value&>()))>, ast_node_type>>
{
    const value* lent = from.current_value();
    if (!lent)
        return std::nullopt;

    auto out = coerce_checked(context, from, *lent, coerce);
    if (out)
        (void) from.next_value();

    return out;
}

JSONV_NODISCARD
std::expected<std::string, ast_node_type> coerce_extract_string(extraction_context& context, reader& from)
{
    if (auto lent = coerce_lent_value(context, from, coerce_string))
        return *std::move(lent);

    switch (from.current().type())
    {
    case ast_node_type::string_canonical:
    case ast_node_type::string_escaped:
    {
        auto out = read_string(context, from);
        if (out)
            (void) from.next_token();

        return out;
    }
    case ast_node_type::literal_null:
    case ast_node_type::literal_true:
    case ast_node_type::literal_false:
    case ast_node_type::integer:
    case ast_node_type::decimal:
    case ast_node_type::object_begin:
    case ast_node_type::array_begin:
        // Anything which is not a string coerces by way of its JSON encoding, which is what `coerce_string` does.
        // Reading the subtree back out is the price of an object or an array; a scalar costs one `value`.
        return to_string(read_value(from));
    default:
        return problem_wrong_type(context, from, "a value");
    }
}

JSONV_NODISCARD
std::expected<bool, ast_node_type> coerce_extract_boolean(extraction_context& context, reader& from)
{
    if (auto lent = coerce_lent_value(context, from, coerce_boolean))
        return *std::move(lent);

    bool out = false;
    switch (from.current().type())
    {
    case ast_node_type::literal_null:
    case ast_node_type::literal_false:
        out = false;
        break;
    case ast_node_type::literal_true:
        out = true;
        break;
    case ast_node_type::object_begin:
    case ast_node_type::array_begin:
    {
        // Both openers carry their element count, so a structure does not have to be walked to be weighed. It does
        // have to be checked: a document which ends part-way through one still has an opener with a count on it, and
        // answering from that count reports the truthiness of a value the source never finished writing. A structure
        // which closed always has a token after it, so a skip which exhausts the reader is one that ran off the end.
        bool empty = is_empty(from.current());
        if (!from.next_value())
        {
            return context.problem(context.problem_path(from),
                                   "Document ended part-way through the value being coerced to a boolean"
                                  );
        }

        return !empty;
    }
    case ast_node_type::string_canonical:
    case ast_node_type::string_escaped:
        out = !is_empty(from.current());
        break;
    case ast_node_type::integer:
        out = !is_zero_integer(from.current());
        break;
    case ast_node_type::decimal:
    {
        auto number = read_decimal(context, from);
        if (!number)
            return std::unexpected(number.error());

        // Negative zero is a zero, which is why this compares rather than looking at the token.
        out = *number != 0.0;
        break;
    }
    default:
        return problem_wrong_type(context, from, "a value");
    }

    // Everything reaching here is a single token; the structures above advance themselves.
    (void) from.next_token();
    return out;
}

/// \c coerce_integer reads a \c decimal by truncating toward zero, clamped to the bounds of \c std::int64_t. A
/// magnitude beyond those bounds has no \c std::int64_t to convert to -- the conversion is undefined rather than
/// saturating -- and NaN has no meaningful clamp at all, so both are decided there rather than by the hardware.
JSONV_NODISCARD
std::expected<std::int64_t, ast_node_type> coerce_decimal_to_integer(extraction_context& context, reader& from)
{
    auto number = read_decimal(context, from);
    if (!number)
        return std::unexpected(number.error());

    return coerce_integer(value(*number));
}

/// Narrow the \c std::int64_t a \c coerce_X produced into the type actually being built, reporting one which does not
/// fit and advancing the reader off the value when it does.
///
/// The coercing rules run through \c std::int64_t because that is what \c coerce_integer returns. What fits the
/// destination is this layer's question, and the answer is a problem rather than a modular wrap.
template <typename T>
JSONV_NODISCARD
std::expected<T, ast_node_type> narrow_checked(extraction_context& context, reader& from, std::int64_t wide)
{
    if (!std::in_range<T>(wide))
    {
        std::ostringstream os;
        os << "Integer " << wide << " is out of range for " << demangle(typeid(T).name());
        return context.problem(context.problem_path(from), std::move(os).str());
    }

    // Everything `coerce_integer` reads is a single token, so this is `next_value` by another name.
    (void) from.next_token();
    return static_cast<T>(wide);
}

template <typename T>
JSONV_NODISCARD
std::expected<T, ast_node_type> coerce_extract_integer(extraction_context& context, reader& from)
{
    if (const value* lent = from.current_value())
    {
        // The lent tree is read with the same range check as a token, so the two sources agree about what fits.
        auto wide = coerce_checked(context, from, *lent, coerce_integer);
        if (!wide)
            return std::unexpected(wide.error());

        return narrow_checked<T>(context, from, *wide);
    }

    std::expected<std::int64_t, ast_node_type> wide;
    switch (from.current().type())
    {
    case ast_node_type::integer:
    {
        // Read against the destination directly, so the range check sees the literal rather than something which has
        // already been squeezed through `std::int64_t`.
        auto out = read_integer<T>(context, from);
        if (out)
            (void) from.next_token();

        return out;
    }
    case ast_node_type::literal_true:
        wide = 1;
        break;
    case ast_node_type::literal_false:
        wide = 0;
        break;
    case ast_node_type::decimal:
        wide = coerce_decimal_to_integer(context, from);
        break;
    case ast_node_type::string_canonical:
    case ast_node_type::string_escaped:
        wide = coerce_from_string(context, from, [] (const value& x) { return coerce_integer(x); });
        break;
    default:
        return problem_wrong_type(context, from, "an integer, a decimal, a string or a boolean");
    }

    if (!wide)
        return std::unexpected(wide.error());

    return narrow_checked<T>(context, from, *wide);
}

JSONV_NODISCARD
std::expected<double, ast_node_type> coerce_extract_decimal(extraction_context& context, reader& from)
{
    if (auto lent = coerce_lent_value(context, from, coerce_decimal))
        return *std::move(lent);

    std::expected<double, ast_node_type> out;
    switch (from.current().type())
    {
    case ast_node_type::integer:
    case ast_node_type::decimal:
        out = read_decimal(context, from);
        break;
    case ast_node_type::literal_true:
        out = 1.0;
        break;
    case ast_node_type::literal_false:
        out = 0.0;
        break;
    case ast_node_type::string_canonical:
    case ast_node_type::string_escaped:
        out = coerce_from_string(context, from, [] (const value& x) { return coerce_decimal(x); });
        break;
    default:
        return problem_wrong_type(context, from, "an integer, a decimal, a string or a boolean");
    }

    if (out)
        (void) from.next_token();

    return out;
}

JSONV_NODISCARD
std::expected<float, ast_node_type> coerce_extract_float(extraction_context& context, reader& from)
{
    auto out = coerce_extract_decimal(context, from);
    if (!out)
        return std::unexpected(out.error());

    return static_cast<float>(*out);
}

template <typename T>
void register_integer_coerce_extractor(formats&              fmt,
                                       duplicate_type_action on_duplicate = duplicate_type_action::exception
                                      )
{
    static auto instance =
        make_extractor([] (extraction_context& context, reader& from)
                       {
                           return coerce_extract_integer<T>(context, from);
                       }
                      );
    fmt.register_extractor(&instance, on_duplicate);
}

formats create_coerce_formats()
{
    formats fmt;

    static auto string_extractor = make_extractor(coerce_extract_string);
    fmt.register_extractor(&string_extractor);

    static auto bool_extractor = make_extractor(coerce_extract_boolean);
    fmt.register_extractor(&bool_extractor);

    register_integer_coerce_extractor<std::int8_t>(fmt);
    register_integer_coerce_extractor<std::uint8_t>(fmt);
    register_integer_coerce_extractor<std::int16_t>(fmt);
    register_integer_coerce_extractor<std::uint16_t>(fmt);
    register_integer_coerce_extractor<std::int32_t>(fmt);
    register_integer_coerce_extractor<std::uint32_t>(fmt);
    register_integer_coerce_extractor<std::int64_t>(fmt);
    register_integer_coerce_extractor<std::uint64_t>(fmt);

    // These common types are usually covered by the explicitly-sized integers, but try to add them for platforms like
    // OSX and Windows.
    register_integer_coerce_extractor<std::size_t>(fmt, duplicate_type_action::ignore);
    register_integer_coerce_extractor<std::ptrdiff_t>(fmt, duplicate_type_action::ignore);
    register_integer_coerce_extractor<long>(fmt, duplicate_type_action::ignore);
    register_integer_coerce_extractor<unsigned long>(fmt, duplicate_type_action::ignore);

    static auto double_extractor = make_extractor(coerce_extract_decimal);
    fmt.register_extractor(&double_extractor);
    static auto float_extractor = make_extractor(coerce_extract_float);
    fmt.register_extractor(&float_extractor);

    return fmt;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The registries themselves                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const formats& default_formats_ref()
{
    static formats instance = create_default_formats();
    return instance;
}

formats& global_formats_ref()
{
    static formats instance = default_formats_ref();
    return instance;
}

const formats& coerce_formats_ref()
{
    static formats instance = formats::compose({ create_coerce_formats(), default_formats_ref() });
    return instance;
}

}

formats formats::defaults()
{
    return formats::compose({ default_formats_ref() });
}

formats formats::global()
{
    return formats::compose({ global_formats_ref() });
}

formats formats::set_global(formats fmt)
{
    using std::swap;

    swap(global_formats_ref(), fmt);
    return fmt;
}

formats formats::reset_global()
{
    return set_global(default_formats_ref());
}

formats formats::coerce()
{
    return formats::compose({ coerce_formats_ref() });
}

}

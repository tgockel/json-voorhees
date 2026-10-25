/// \file
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/value.hpp>
#include <jsonv/algorithm.hpp>
#include <jsonv/encode.hpp>
#include <jsonv/path.hpp>

#include "array_impl.hpp"
#include "char_convert.hpp"
#include "detail.hpp"
#include "object_impl.hpp"
#include "string_impl.hpp"
#include "value_access.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// kind_error                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

kind_error::kind_error(const std::string& description) :
        std::logic_error(description)
{ }

kind_error::~kind_error() noexcept
{ }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// value                                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// A string is copied once on its way into a `value`, into the value itself when it is short enough and otherwise into a
// block of its own. The wide constructors convert first and copy the result. Anything which can throw happens before the
// value is changed, so a failure leaves nothing to free (#272).

value::value(const std::string& val) :
        value(std::string_view(val))
{ }

value::value(const std::string_view& val)
{
    detail::value_access::set_string(*this, val);
}

value::value(const char* val) :
        value(std::string_view(val))
{ }

value::value(const std::wstring& val)
{
    detail::value_access::set_string(*this, detail::convert_to_narrow(val));
}

value::value(const wchar_t* val)
{
    detail::value_access::set_string(*this, detail::convert_to_narrow(val));
}

value::value(int64_t val)
{
    _repr.words = { jsonv::kind::integer, 0U, { .integer = val } };
}

value::value(double val)
{
    _repr.words = { jsonv::kind::decimal, 0U, { .decimal = val } };
}

value::value(float val) :
        value(double(val))
{ }

value::value(bool val)
{
    _repr.words = { jsonv::kind::boolean, 0U, { .boolean = val } };
}

#define JSONV_VALUE_INTEGER_ALTERNATIVE_CTOR_GENERATOR(type_)                      \
    value::value(type_ val)                                                        \
    {                                                                              \
        _repr.words = { jsonv::kind::integer, 0U, { .integer = int64_t(val) } };   \
    }
JSONV_INTEGER_ALTERNATES_LIST(JSONV_VALUE_INTEGER_ALTERNATIVE_CTOR_GENERATOR)

value::~value() noexcept
{
    clear();
}

value::value(const value& other)
{
    switch (other.kind())
    {
    case jsonv::kind::object:
        _repr.words = { jsonv::kind::object, 0U, { .object = detail::object_impl::copy(other._repr.words.data.object) } };
        break;
    case jsonv::kind::array:
        _repr.words = { jsonv::kind::array, 0U, { .array = detail::array_impl::copy(other._repr.words.data.array) } };
        break;
    case jsonv::kind::string:
        if (other._repr.words.string_length == detail::value_string_out_of_line)
            detail::value_access::set_string(*this, other._repr.words.data.string->view());
        else
            _repr = other._repr;
        break;
    case jsonv::kind::integer:
    case jsonv::kind::decimal:
    case jsonv::kind::boolean:
    case jsonv::kind::null:
        _repr = other._repr;
        break;
    }
}

value& value::operator=(const value& other)
{
    value copy(other);
    swap(copy);
    return *this;
}

value::value(value&& other) noexcept :
        _repr(other._repr)
{
    other._repr = detail::value_representation();
}

value& value::operator=(value&& source) noexcept
{
    if (this != &source)
    {
        clear();

        _repr        = source._repr;
        source._repr = detail::value_representation();
    }

    return *this;
}

bool value::is_array() const
{
    return kind() == jsonv::kind::array;
}

bool value::is_boolean() const
{
    return kind() == jsonv::kind::boolean;
}

bool value::is_decimal() const
{
    return kind() == jsonv::kind::decimal || kind() == jsonv::kind::integer;
}

bool value::is_integer() const
{
    return kind() == jsonv::kind::integer;
}

bool value::is_null() const
{
    return kind() == jsonv::kind::null;
}

bool value::is_string() const
{
    return kind() == jsonv::kind::string;
}

bool value::is_object() const
{
    return kind() == jsonv::kind::object;
}

template <typename TValueRef, typename TPathIterator, typename FOnNonexistantPath>
TValueRef walk_path(TValueRef&&               current,
                    TPathIterator             first,
                    TPathIterator             last,
                    const FOnNonexistantPath& on_nonexistant_path
                   )
{
    if (first == last)
        return current; // NOLINT(bugprone-return-const-ref-from-parameter): a node of the tree, never a temporary

    const path_element& elem = *first;
    switch (elem.kind())
    {
    case path_element_kind::array_index:
        check_type({ jsonv::kind::array, jsonv::kind::null }, current.kind());
        if (current.kind() != jsonv::kind::array || elem.index() >= current.size())
            on_nonexistant_path(elem, current);
        return walk_path(current.at(elem.index()), std::next(first), last, on_nonexistant_path);
    case path_element_kind::object_key:
        check_type({ jsonv::kind::object, jsonv::kind::null }, current.kind());
        if (current.kind() != jsonv::kind::object || !current.count(elem.key()))
            on_nonexistant_path(elem, current);
        return walk_path(current.at(elem.key()), std::next(first), last, on_nonexistant_path);
    default:
        throw std::runtime_error(to_string(elem));
    }
}

value& value::at_path(const jsonv::path& p)
{
    return walk_path(*this,
                     p.begin(),
                     p.end(),
                     [&p] (const path_element& elem, const value& current)
                     {
                         throw std::out_of_range(to_string(elem) + " does not exist on " + to_string(current)
                                                 + " (full path: " + to_string(p) + ")"
                                                );
                     }
                    );
}

value& value::at_path(std::string_view path_description)
{
    return at_path(jsonv::path::create(path_description));
}

value& value::at_path(size_type path_idx)
{
    return at_path(jsonv::path({ path_element(path_idx) }));
}

const value& value::at_path(const jsonv::path& p) const
{
    return walk_path(*this,
                     p.begin(),
                     p.end(),
                     [&p] (const path_element& elem, const value& current)
                     {
                         throw std::out_of_range(to_string(elem) + " does not exist on " + to_string(current)
                                                 + " (full path: " + to_string(p) + ")"
                                                );
                     }
                    );
}

const value& value::at_path(std::string_view path_description) const
{
    return at_path(jsonv::path::create(path_description));
}

const value& value::at_path(size_type path_idx) const
{
    return at_path(jsonv::path({ path_element(path_idx) }));
}

namespace
{

/// The child of \a current which \a elem names, or null if there is none. This is the question \c walk_path asks before
/// it calls its \c on_nonexistant_path, with the answer handed back instead of acted on.
///
/// \throws kind_error if \a elem cannot name a child of \a current at all.
value* existing_child(value& current, const path_element& elem)
{
    switch (elem.kind())
    {
    case path_element_kind::array_index:
        check_type({ jsonv::kind::array, jsonv::kind::null }, current.kind());
        if (current.kind() != jsonv::kind::array || elem.index() >= current.size())
            return nullptr;
        return &current.at(elem.index());
    case path_element_kind::object_key:
        check_type({ jsonv::kind::object, jsonv::kind::null }, current.kind());
        if (current.kind() != jsonv::kind::object || !current.count(elem.key()))
            return nullptr;
        return &current.at(elem.key());
    default:
        throw std::runtime_error(to_string(elem));
    }
}

/// Gives \a node the element \a elem names, as a \c null, and returns it. A \c null \a node becomes the container
/// \a elem indexes. \a elem must not name an element \a node already has, and an array index must be one an array can
/// hold: for the largest \c std::size_t, `index + 1` wraps to 0 and the \c resize would empty the array.
///
/// If this throws, \a node is as it was. A \c null \a node only becomes a container after the container has its
/// element, and the element is added by one \c resize or \c operator[], which have no effect when they throw.
value& open_slot(value& node, const path_element& elem)
{
    value  staged;
    value* container = &node;
    value* slot      = nullptr;
    switch (elem.kind())
    {
    case path_element_kind::array_index:
        if (node.kind() == jsonv::kind::null)
        {
            staged    = array();
            container = &staged;
        }
        container->resize(elem.index() + 1);
        slot = &container->at(elem.index());
        break;
    case path_element_kind::object_key:
        if (node.kind() == jsonv::kind::null)
        {
            staged    = object();
            container = &staged;
        }
        slot = &(*container)[elem.key()];
        break;
    default:
        throw std::runtime_error(to_string(elem));
    }

    // Moving hands over the array or object without relocating what is in it, so `slot` is still where it was.
    if (container == &staged)
        node = std::move(staged);
    return *slot;
}

}

value& value::path(const jsonv::path& p)
{
    // Follow what already exists. Nothing is changed here, and a kind_error can only come from here: below the first
    // missing element everything is created as a null, which any element can be taken from.
    value* current = this;
    auto   it      = p.begin();
    for (; it != p.end(); ++it)
    {
        value* child = existing_child(*current, *it);
        if (!child)
            break;
        current = child;
    }
    if (it == p.end())
        return *current;

    // No array can hold `max + 1` elements, so an index of `max` never exists, and `index + 1` would wrap to 0. Refuse
    // it before anything is built. The missing part is built before any of it is attached, so refusing at the index
    // itself would let a large but valid index ahead of it allocate its whole array first, to throw it away. An index
    // of `max` cannot be in what exists, so the part which is missing is all there is to look at.
    for (auto missing = it; missing != p.end(); ++missing)
    {
        if (missing->kind() == path_element_kind::array_index
            && missing->index() == std::numeric_limits<std::size_t>::max())
            throw std::length_error("no array can hold " + to_string(*missing) + " (full path: " + to_string(p) + ")");
    }

    // Build the rest of the path apart from the tree, so a failure part of the way through has nothing to undo...
    value  tail;
    value* leaf = &tail;
    for (auto rest = std::next(it); rest != p.end(); ++rest)
        leaf = &open_slot(*leaf, *rest);

    // ...then hang it where the first missing element goes. That is the only change to the tree: `open_slot` either
    // throws and leaves `*current` as it was or returns, and moving a `value` cannot throw.
    const bool leaf_is_slot = leaf == &tail;
    value&     slot         = open_slot(*current, *it);
    slot = std::move(tail);
    return leaf_is_slot ? slot : *leaf;
}

value& value::path(std::string_view path_description)
{
    return path(jsonv::path::create(path_description));
}

value& value::path(size_type path_idx)
{
    return path(jsonv::path({ path_element(path_idx) }));
}

value::size_type value::count_path(const jsonv::path& p) const
{
    // TODO(#148): Performance of this function sucks!
    try
    {
        // Called for the throw -- presence is the answer, not the value found.
        (void) at_path(p);
        return 1;
    }
    catch (const std::out_of_range&)
    {
        return 0;
    }
    catch (const kind_error&)
    {
        return 0;
    }
}

value::size_type value::count_path(std::string_view p) const
{
    return count_path(jsonv::path::create(p));
}

value::size_type value::count_path(size_type p) const
{
    return count_path(jsonv::path({ p }));
}

void value::swap(value& other) noexcept
{
    using std::swap;

    // Both layouts are trivially copyable, so swapping the bytes swaps the values.
    swap(_repr, other._repr);
}

void value::clear()
{
    switch (_repr.words.kind)
    {
    case jsonv::kind::object:
        detail::object_impl::destroy(_repr.words.data.object);
        break;
    case jsonv::kind::array:
        detail::array_impl::destroy(_repr.words.data.array);
        break;
    case jsonv::kind::string:
        if (_repr.words.string_length == detail::value_string_out_of_line)
            detail::string_impl::destroy(_repr.words.data.string);
        break;
    case jsonv::kind::integer:
    case jsonv::kind::decimal:
    case jsonv::kind::boolean:
    case jsonv::kind::null:
        // do nothing
        break;
    }

    _repr = detail::value_representation();
}

std::string_view value::as_string() const
{
    check_type(jsonv::kind::string, _repr.words.kind);
    return detail::value_access::string_view(*this);
}

std::string_view value::as_string_view() const &
{
    return as_string();
}

std::wstring value::as_wstring() const
{
    return detail::convert_to_wide(as_string());
}

int64_t value::as_integer() const
{
    check_type(jsonv::kind::integer, _repr.words.kind);
    return _repr.words.data.integer;
}

double value::as_decimal() const
{
    if (_repr.words.kind == jsonv::kind::integer)
        return double(as_integer());
    check_type(jsonv::kind::decimal, _repr.words.kind);
    return _repr.words.data.decimal;
}

bool value::as_boolean() const
{
    check_type(jsonv::kind::boolean, _repr.words.kind);
    return _repr.words.data.boolean;
}

bool value::operator==(const value& other) const
{
    if (this == &other && kind_valid(kind()))
        return true;
    else
        return compare(other) == 0;
}

bool value::operator !=(const value& other) const
{
    // must be first: an invalid type is not equal to itself
    if (!kind_valid(kind()))
        return true;

    if (this == &other)
        return false;
    else
        return compare(other) != 0;
}

int value::compare(const value& other) const
{
    using jsonv::compare;

    return compare(*this, other);
}

bool value::operator< (const value& other) const
{
    return compare(other) < 0;
}

bool value::operator<=(const value& other) const
{
    return compare(other) <= 0;
}

bool value::operator> (const value& other) const
{
    return compare(other) > 0;
}

bool value::operator>=(const value& other) const
{
    return compare(other) >= 0;
}

std::ostream& operator<<(std::ostream& stream, const value& val)
{
    ostream_encoder encoder(stream);
    encoder.encode(val);
    return stream;
}

std::string to_string(const value& val)
{
    std::ostringstream os;
    os << val;
    return os.str();
}

bool value::empty() const noexcept
{
    switch (kind())
    {
    case jsonv::kind::object:
        return detail::object_impl::size(_repr.words.data.object) == 0U;
    case jsonv::kind::array:
        return detail::array_impl::size(_repr.words.data.array) == 0U;
    case jsonv::kind::string:
        return detail::value_access::string_view(*this).empty();
    case jsonv::kind::null:
        return true; // by definition a null value is empty
    case jsonv::kind::integer:
    case jsonv::kind::decimal:
    case jsonv::kind::boolean:
        return false; // in the sense that they contain a value
    default:
        // Should never hit this...
        return true;
    }
}

value::size_type value::size() const
{
    check_type({ jsonv::kind::object, jsonv::kind::array, jsonv::kind::string }, kind());

    switch (kind())
    {
    case jsonv::kind::object:
        return detail::object_impl::size(_repr.words.data.object);
    case jsonv::kind::array:
        return detail::array_impl::size(_repr.words.data.array);
    case jsonv::kind::string:
        return detail::value_access::string_view(*this).size();
    case jsonv::kind::integer:
    case jsonv::kind::decimal:
    case jsonv::kind::boolean:
    case jsonv::kind::null:
    default:
        // Should never hit this...
        return 0;
    }
}

value value::map(const std::function<value (const value&)>& func) const&
{
    return jsonv::map(func, *this);
}

value value::map(const std::function<value (value)>& func) &&
{
    return jsonv::map(func, std::move(*this));
}

void swap(value& a, value& b) noexcept
{
    a.swap(b);
}

// There are no static initialization issues here -- the memory of a static variable starts as all 0, which is identical
// to a value with kind::null.
const value null = value();

}

namespace std
{

template <typename TForwardIterator, typename FHasher>
static std::size_t hash_range(TForwardIterator first, TForwardIterator last, const FHasher& hasher)
{
    std::size_t x = 0;
    for ( ; first != last; ++first)
        x = (x << 1) ^ hasher(*first);

    return x;
}

size_t hash<jsonv::value>::operator()(const jsonv::value& val) const noexcept
{
    using namespace jsonv;

    switch (val.kind())
    {
    case jsonv::kind::object:
        return hash_range(val.begin_object(), val.end_object(),
                          [] (value::const_object_reference x) { return std::hash<std::string>()(x.first)
                                                                      ^ std::hash<value>()(x.second);
                                                               }
                         );
    case jsonv::kind::array:
        return hash_range(val.begin_array(), val.end_array(), hash<jsonv::value>());
    case jsonv::kind::string:
        return std::hash<std::string_view>()(val.as_string());
    case jsonv::kind::integer:
    case jsonv::kind::decimal:
        {
            // An integer compares equal to the decimal of the same number (see compare), so both kinds hash through
            // as_decimal -- otherwise equal keys could land in different unordered_map buckets. Distinct integers past
            // 2^53 which convert to the same double share a hash, which is allowed. All NaNs compare equal regardless
            // of sign or payload (see compare_traits::compare_decimals), so they must share a hash too.
            const double d = val.as_decimal();
            return std::isnan(d) ? 0x7ff8000000000000ULL : std::hash<double>()(d);
        }
    case jsonv::kind::boolean:
        return std::hash<bool>()(val.as_boolean());
    case jsonv::kind::null:
        return 0x51afb2fe9467d0f7ULL;
    default:
        // Should never hit this...
        return 0ULL;
    }
}

}

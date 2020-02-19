/// \file
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/parse.hpp>
#include <jsonv/parse_index.hpp>

#include <cassert>
#include <cstring>
#include <malloc.h>
#include <new>
#include <sstream>
#include <utility>

#include "detail/match/number.hpp"
#include "detail/match/string.hpp"

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Private Helper Functions                                                                                           //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef JSONV_AST_PARSE_MAX_DEPTH
#   define JSONV_AST_PARSE_MAX_DEPTH 128
#endif

static inline ast_node_type ast_node_type_from_coded(std::uint64_t src)
{
    return static_cast<ast_node_type>(src & 0xff);
}

static inline std::uint64_t encode(ast_node_type token, const char* src_location)
{
    auto src_index = reinterpret_cast<std::uintptr_t>(src_location);
    static_assert(sizeof src_index <= sizeof(std::uint64_t), "!!");
    return std::uint64_t(static_cast<std::uint8_t>(token))
         | (std::uint64_t(src_index) << 8);
}

static constexpr std::uintptr_t ast_node_prefix_add =
    sizeof(std::uintptr_t) == sizeof(std::uint64_t) ? std::uintptr_t(1UL << 56) : 0;

static constexpr std::uintptr_t ast_node_prefix_from_ptr(const char* src_begin)
{
    if constexpr (sizeof(std::uintptr_t) == sizeof(std::uint64_t))
    {
        constexpr std::uintptr_t mask = std::uintptr_t(0xffUL << 56);
        return reinterpret_cast<std::uintptr_t>(src_begin) & mask;
    }
    else
    {
        return 0;
    }
}

static inline std::pair<ast_node_type, const char*> decode_ast_node_position(std::uintptr_t prefix, std::uint64_t src)
{
    static_assert(sizeof prefix <= sizeof src);

    auto ptr = std::uintptr_t(src >> 8) | prefix;
    return { ast_node_type_from_coded(src), reinterpret_cast<const char*>(ptr) };
}

static inline std::size_t code_size(ast_node_type src /* UNSAFE */)
{
    static const std::size_t jumps[] =
    {
        1, // $
        3, // ^  { end_idx, element_count }
        3, // {  { end_idx, element_count }
        1, // }
        3, // [  { end_idx, element_count }
        1, // ]
        2, // s  { encoded_size }
        2, // S  { encoded_size }
        2, // k  { encoded_size }
        2, // K  { encoded_size }
        1, // t
        1, // f
        1, // n
        2, // i  { run_length }
        2, // d  { run_length }
        2, // !  { error_code }
    };
    return jumps[static_cast<std::uint8_t>(src)];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ast_exception                                                                                                      //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct JSONV_LOCAL ast_exception
{
    ast_error   code;
    std::size_t index;

    explicit constexpr ast_exception(ast_error code, std::size_t index)
            : code(code)
            , index(index)
    { }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse_index                                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct JSONV_LOCAL parse_index::impl final
{
    const char*   src_begin;         //!< The source string this AST was built from
    std::uint64_t data_size;         //!< Number of used elements in \c data.
    std::uint64_t data_capacity;     //!< Capacity of \c data.
    ast_error     first_error_code;  //!< The code of the first-encountered error
    std::uint64_t first_error_index; //!< The index of the first error encountered in parsing.

    static impl* allocate(std::size_t capacity)
    {
        if (capacity < 16U)
            capacity = 16U;

        auto alloc_sz = sizeof(impl) + capacity * sizeof(std::uint64_t);
        if (void* p = std::aligned_alloc(alignof(impl), alloc_sz))
        {
            auto out = reinterpret_cast<impl*>(p);
            out->src_begin         = nullptr;
            out->data_size         = 0U;
            out->data_capacity     = capacity;
            out->first_error_code  = ast_error::none;
            out->first_error_index = 0U;
            return out;
        }
        else
        {
            throw std::bad_alloc();
        }
    }

    std::uint64_t& data(std::size_t idx)
    {
        auto data_ptr = reinterpret_cast<std::uint64_t*>(this + 1);
        return data_ptr[idx];
    }

    static void destroy(impl* p)
    {
        std::free(p);
    }

    static void grow_data_buffer(impl*& self)
    {
        // NOTE: Doubling the data capacity will always grow enough to hold a complete code size, as the minimum size
        // can hold the largest code size.
        auto new_capacity = self->data_capacity * 2;

        // OPTIMIZATION: It's possible that `realloc` would be better here, but there is not an aligned version of that.
        auto new_self = allocate(new_capacity);
        std::memcpy(new_self, self, sizeof *new_self + self->data_size * sizeof self->data(0));
        new_self->data_capacity = new_capacity;

        destroy(self);
        self = new_self;
    }

    /// Add the \a token, \a src_location pair to the \a self \c data buffer.
    ///
    /// \param self Like \c this, but will be updated in the case that \c data_capacity cannot store the inserted token.
    /// \returns The index that was inserted into (\c data_size before the insertion was performed). This is used for
    ///  adding extra data elements when \a token is something like \c ast_node_type::object_begin.
    static inline std::size_t push_back(impl*& self, ast_node_type token, const char* src_location)
    {
        auto added_sz = code_size(token);
        if (self->data_size + added_sz > self->data_capacity)
        {
            JSONV_UNLIKELY;
            grow_data_buffer(self);
        }

        auto put_idx = self->data_size;
        self->data(put_idx) = encode(token, src_location);
        self->data_size += added_sz;
        return put_idx;
    }

    static ast_exception push_error(impl*& self, ast_error error_code, const char* begin, const char* src_location)
    {
        auto idx = push_back(self, ast_node_type::error, src_location);
        self->data(idx + 1) = static_cast<std::uint64_t>(error_code);

        self->first_error_code  = error_code;
        self->first_error_index = src_location - begin;
        return ast_exception(error_code, self->first_error_index);
    }

    static void parse(impl*& self, string_view src);

    template <std::size_t N>
    static void parse_literal(impl*&        self,
                              const char*   begin,
                              const char*&  iter,
                              const char*   end,
                              const char (& expected_token)[N],
                              ast_node_type success_value
                             );
};

// OPTIMIZATION(SIMD-SSE4.2): Skip over chunks with cmpistri
static void fastforward_whitespace(const char*& iter, const char* end)
{
    while (iter < end)
    {
        char c = *iter;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            ++iter;
        else
            break;
    }
}

static bool fastforward_comment(const char*& ext_iter, const char* end)
{
    assert(ext_iter[0] == '/');
    if (ext_iter + 1 == end || ext_iter[1] != '*')
        return false;

    for (const char* iter = ext_iter + 2; iter < end; ++iter)
    {
        if (iter[0] == '*')
        {
            if (iter + 1 == end)
            {
                return false;
            }
            else if (iter[1] == '/')
            {
                ext_iter = iter + 2;
                return true;
            }
        }
    }
    return false;
}

// OPTIMIZATION(SIMD): Tokens can be checked as a single `uint32_t`, including "false", as 'f' has already been checked.
template <std::size_t NPlusOne>
void parse_index::impl::parse_literal(impl*&        self,
                                      const char*   begin,
                                      const char*&  iter,
                                      const char*   end,
                                      const char (& expected_token)[NPlusOne],
                                      ast_node_type success_value
                                     )
{
    static constexpr std::size_t N = NPlusOne - 1;

    if (iter + N <= end)
    {
        // start at `idx` 1 because `parse` checks the first value
        for (std::size_t idx = 1; idx < N; ++idx)
        {
            if (iter[idx] != expected_token[idx])
            {
                JSONV_UNLIKELY
                throw push_error(*&self, ast_error::invalid_literal, begin, iter);
            }
        }

        push_back(self, success_value, iter);
        iter += N;
    }
    else
    {
        JSONV_UNLIKELY
        throw push_error(*&self, ast_error::eof, begin, iter);
    }
}

void parse_index::impl::parse(impl*& self, string_view src)
{
    enum class container_state
    {
        item_finished = 0,  // <- an item just finished parsing (value 0 because it is set frequently)
        opened,             // <- just encountered [ or {
        needs_item,         // <- just parsed a `,` or ':', so we need an item (can't close now)
        none,               // <- not in a container
    };

    struct structure_state
    {
        std::size_t   open_index;
        std::size_t   item_count;
        ast_node_type open_token;

        static structure_state create(std::size_t index, ast_node_type opener) JSONV_ALWAYS_INLINE
        {
            return { index, 0UL, opener };
        }
    };

    static constexpr std::size_t max_depth    = std::size_t(JSONV_AST_PARSE_MAX_DEPTH);
    structure_state              structure[max_depth];
    std::size_t                  depth        = 0;
    ast_node_type                container    = ast_node_type::error;
    container_state              state        = container_state::none;

    const char* const begin = src.data();
    const char* iter        = begin;
    const char* const end   = begin + src.size();

    auto push_back_deeper =
        [&](ast_node_type token, const char* src_location) JSONV_ALWAYS_INLINE
        {
            if (depth + 1 > max_depth)
            {
                JSONV_UNLIKELY
                throw push_error(self, ast_error::depth_exceeded, begin, src_location);
            }

            structure[depth] = structure_state::create(push_back(self, token, src_location), token);
            container        = token;
            ++depth;
        };

    auto push_back_out =
        [&](ast_node_type token, ast_node_type expected_open_token, const char* src_location) JSONV_ALWAYS_INLINE
        {
            if (depth <= 0)
            {
                JSONV_UNLIKELY
                throw push_error(self, ast_error::extra_close, begin, src_location);
            }

            auto this_idx = push_back(self, token, src_location);
            --depth;

            // Since we add to `item_count` on ',' tokens, we will undercount by 1, since the last item did not have a
            // comma. Check that we are in `item_finished` state so empty containers remain at 0.
            if (state == container_state::item_finished)
                ++structure[depth].item_count;

            auto open_token = structure[depth].open_token;
            if (open_token != expected_open_token)
            {
                JSONV_UNLIKELY
                throw push_error(self, ast_error::mismatched_close, begin, src_location);
            }

            if (depth > 0)
            {


                container = structure[depth - 1].open_token;

                if (depth == 1U)
                {
                    ++src_location;
                    fastforward_whitespace(*&src_location, end);
                    if (src_location != end && *src_location)
                        throw push_error(self, ast_error::expected_eof, begin, src_location);
                }
            }
            else
            {
                container = ast_node_type::error;
            }

            self->data(structure[depth].open_index + 1) = this_idx;
            self->data(structure[depth].open_index + 2) = structure[depth].item_count;
        };

    auto get_string = [&](ast_node_type token_ascii, ast_node_type token_escaped) JSONV_ALWAYS_INLINE
        {
            if (*iter != '\"')
                throw push_error(self, ast_error::expected_string, begin, iter);

            auto result = detail::match_string(iter, end);
            if (!result)
                throw push_error(self, ast_error::eof, begin, iter);

            auto idx = push_back(self, result.needs_conversion ? token_escaped : token_ascii, iter);
            self->data(idx + 1) = result.length;

            iter += result.length;
        };

    auto get_key = [&]() JSONV_ALWAYS_INLINE
        {
            fastforward_whitespace(*&iter, end);
            if (iter >= end)
                throw push_error(self, ast_error::eof, begin, iter);
            else if (*iter == '}')
                return false;

            get_string(ast_node_type::key_canonical, ast_node_type::key_escaped);

            fastforward_whitespace(*&iter, end);
            if (iter >= end)
                throw push_error(self, ast_error::eof, begin, iter);

            if (*iter != ':')
                throw push_error(self, ast_error::expected_key_delimiter, begin, iter);
            ++iter;
            return true;
        };

    push_back_deeper(ast_node_type::document_start, iter);

    while (iter < end && *iter)
    {
        switch (*iter)
        {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            fastforward_whitespace(*&iter, end);
            break;
        case 't':
            parse_literal(self, begin, *&iter, end, "true", ast_node_type::literal_true);
            state = container_state::item_finished;
            break;
        case 'f':
            parse_literal(self, begin, *&iter, end, "false", ast_node_type::literal_false);
            state = container_state::item_finished;
            break;
        case 'n':
            parse_literal(self, begin, *&iter, end, "null", ast_node_type::literal_null);
            state = container_state::item_finished;
            break;
        case '[':
            push_back_deeper(ast_node_type::array_begin, iter);
            state = container_state::opened;
            ++iter;
            break;
        case ']':
            if (state == container_state::needs_item)
                throw push_error(self, ast_error::close_after_comma, begin, iter);

            push_back_out(ast_node_type::array_end, ast_node_type::array_begin, iter);
            state = container_state::item_finished;
            ++iter;
            break;
        case '{':
            push_back_deeper(ast_node_type::object_begin, iter);
            state = container_state::opened;
            ++iter;
            get_key();
            break;
        case '}':
            if (state == container_state::needs_item)
                throw push_error(self, ast_error::close_after_comma, begin, iter);

            push_back_out(ast_node_type::object_end, ast_node_type::object_begin, iter);
            state = container_state::item_finished;
            ++iter;
            break;
        case ',':
            if (state != container_state::item_finished)
                throw push_error(self, ast_error::unexpected_comma, begin, iter);

            ++iter;
            ++structure[depth - 1].item_count;

            if (container == ast_node_type::object_begin)
                get_key();
            state = container_state::needs_item;
            break;
        case '\"':
            get_string(ast_node_type::string_canonical, ast_node_type::string_escaped);
            state = container_state::item_finished;
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '-':
            if (auto result = detail::match_number(iter, end))
            {
                auto idx = push_back(self, result.decimal ? ast_node_type::decimal : ast_node_type::integer, iter);
                self->data(idx + 1) = result.length;
                iter               += result.length;
                state               = container_state::item_finished;
            }
            else
            {
                throw push_error(self, ast_error::invalid_number, begin, iter);
            }
            break;
        case '/':
            if (fastforward_comment(*&iter, end))
            {
                break;
            }
            else
            {
                throw push_error(self, ast_error::invalid_comment, begin, iter);
            }
        default:
            throw push_error(self, ast_error::unexpected_token, begin, iter);
            break;
        }
    }

    push_back_out(ast_node_type::document_end, ast_node_type::document_start, iter);
}

parse_index::parse_index(impl* ptr) noexcept :
        _impl(ptr)
{ }

parse_index::~parse_index() noexcept
{
    reset();
}

void parse_index::reset()
{
    if (auto p = std::exchange(_impl, nullptr))
    {
        impl::destroy(p);
    }
}

bool parse_index::success() const noexcept
{
    return _impl ? _impl->first_error_code == ast_error::none : false;
}

void parse_index::validate() const
{
    if (!_impl)
        throw std::invalid_argument("AST index was not initialized");
    if (success())
        return;

    throw parse_error({ parse_error::problem(0, 0, _impl->first_error_index, std::string(to_string(_impl->first_error_code))) }, value());
}

parse_index::iterator parse_index::begin() const
{
    if (_impl)
    {
        return iterator(ast_node_prefix_from_ptr(_impl->src_begin), &_impl->data(0));
    }
    else
    {
        return iterator(0, reinterpret_cast<const std::uint64_t*>(0));
    }
}

parse_index::iterator parse_index::end() const
{
    if (_impl)
    {
        return iterator(ast_node_prefix_from_ptr(_impl->src_begin), &_impl->data(_impl->data_size));
    }
    else
    {
        return iterator(0, reinterpret_cast<const std::uint64_t*>(0));
    }
}

parse_index parse_index::parse(string_view src, optional<std::size_t> initial_buffer_capacity)
{
    // OPTIMIZATION: Better heuristics on initial buffer capacity.
    auto p       = impl::allocate(initial_buffer_capacity.value_or(src.size() / 16));
    p->src_begin = src.data();

    try
    {
        impl::parse(p, src);
        return parse_index(p);
    }
    catch (const ast_exception&)
    {
        return parse_index(p);
    }
    catch (...)
    {
        impl::destroy(p);
        throw;
    }
}

std::ostream& operator<<(std::ostream& os, const parse_index& self)
{
    if (!self._impl)
        return os << ast_node_type::error;

    for (const auto& tok : self)
    {
        os << tok.type();
    }

    return os;
}

std::string to_string(const parse_index& self)
{
    std::ostringstream ss;
    ss << self;
    return std::move(ss).str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse_index::iterator                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

parse_index::iterator& parse_index::iterator::operator++()
{
    auto [prev_token, prev_ptr] = decode_ast_node_position(_prefix, *_iter);

    _iter += code_size(prev_token);
    auto new_ptr = decode_ast_node_position(_prefix, *_iter).second;

    if (new_ptr < prev_ptr)
        _prefix += ast_node_prefix_add;

    return *this;
}

ast_node parse_index::iterator::operator*() const
{
    auto [token, ptr] = decode_ast_node_position(_prefix, *_iter);

    switch (token)
    {
    case ast_node_type::document_start:   return ast_node::document_start(ptr);
    case ast_node_type::document_end:     return ast_node::document_end(ptr);
    case ast_node_type::object_begin:     return ast_node::object_begin(ptr, _iter[2]);
    case ast_node_type::object_end:       return ast_node::object_end(ptr);
    case ast_node_type::array_begin:      return ast_node::array_begin(ptr, _iter[2]);
    case ast_node_type::array_end:        return ast_node::array_end(ptr);
    case ast_node_type::string_canonical: return ast_node::string_canonical(ptr, _iter[1]);
    case ast_node_type::string_escaped:   return ast_node::string_escaped(ptr, _iter[1]);
    case ast_node_type::key_canonical:    return ast_node::key_canonical(ptr, _iter[1]);
    case ast_node_type::key_escaped:      return ast_node::key_escaped(ptr, _iter[1]);
    case ast_node_type::literal_true:     return ast_node::literal_true(ptr);
    case ast_node_type::literal_false:    return ast_node::literal_false(ptr);
    case ast_node_type::literal_null:     return ast_node::literal_null(ptr);
    case ast_node_type::integer:          return ast_node::integer(ptr, _iter[1]);
    case ast_node_type::decimal:          return ast_node::decimal(ptr, _iter[1]);
    case ast_node_type::error:            return ast_node::error(ptr, 1U, static_cast<ast_error>(_iter[1]));
    default:                              throw std::invalid_argument(std::string("Unknown token: ") + to_string(token));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse                                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{

template <typename... F>
struct overload : F...
{
    using F::operator()...;
};

template <typename... F>
overload(F...) -> overload<F...>;

value extract_single(parse_index::const_iterator& iter, parse_index::const_iterator last);

value extract_object(parse_index::const_iterator& iter, parse_index::const_iterator last)
{
    auto first_token = *iter;
    if (first_token.type() != ast_node_type::object_begin)
        throw std::invalid_argument("`extract_object` called on non-object token");

    value out = object();

    ++iter;
    while (iter != last)
    {
        auto key_token = *iter;
        if (key_token.type() == ast_node_type::object_end)
        {
            return out;
        }

        std::string key = key_token.visit(overload
            {
                [](const ast_node::key_canonical& x) { return std::string(x.value()); },
                [](const ast_node::key_escaped&   x) { return x.value(); },
                [](const auto&) -> std::string
                {
                    // This shouldn't happen if the AST index is valid
                    JSONV_UNLIKELY
                    throw std::invalid_argument("Extracting object does not have repeating KEY, VALUE sequence");
                }
            });

        ++iter;
        value val = extract_single(*&iter, last);

        out.insert({ std::move(key), std::move(val) });
        ++iter;
    }

    throw std::invalid_argument("Did not find end of object");
}

value extract_array(parse_index::const_iterator& iter, parse_index::const_iterator last)
{
    auto first_token = *iter;
    value out        = first_token.visit(overload
        {
            [](const ast_node::array_begin& arr) -> value
            {
                auto out = array();
                out.reserve(arr.element_count());
                return out;
            },
            [](const auto&) -> value
            {
                JSONV_UNLIKELY
                throw std::invalid_argument("Extracting array does not start with [");
            }
        });

    ++iter;
    while (iter != last)
    {
        auto token = *iter;
        if (token.type() == ast_node_type::array_end)
            return out;

        out.push_back(extract_single(*&iter, last));
        ++iter;
    }

    throw std::invalid_argument("Did not find end of array");
}

value extract_single(parse_index::const_iterator& iter, parse_index::const_iterator last)
{
    if (iter == last)
        throw std::invalid_argument("Can not extract from empty");

    auto node = *iter;
    return node.visit(
        overload
        {
            [&](const ast_node::object_begin&)       -> value { return extract_object(*&iter, last); },
            [&](const ast_node::array_begin&)        -> value { return extract_array(*&iter, last); },
            [&](const ast_node::integer& x)          -> value { return x.value(); },
            [&](const ast_node::decimal& x)          -> value { return x.value(); },
            [&](const ast_node::string_canonical& x) -> value { return x.value(); },
            [&](const ast_node::string_escaped& x)   -> value { return x.value(); },
            [&](const ast_node::literal_false& x)    -> value { return x.value(); },
            [&](const ast_node::literal_true& x)     -> value { return x.value(); },
            [&](const ast_node::literal_null& x)     -> value { return x.value(); },
            [&](const ast_node::error& x)         -> value
            {
                throw parse_error({ { 0, 0, 0, to_string(x.error_code()) } }, null);
            },
            [&](const auto& x) -> value
            {
                // If the AST if valid, this should not be hit
                throw std::invalid_argument(std::string("unexpected token ") + to_string(x.type()));
            },
        });
}

}

value parse_index::extract_tree(const parse_options& options /* TODO(#145) */) const
{
    if (!_impl)
        throw std::invalid_argument("AST index was not initialized");

    auto iter = begin();
    auto last = end();

    if (iter == last)
        return null;

    auto first_node = *iter;
    if (first_node.type() != ast_node_type::document_start)
        throw std::invalid_argument("AST index did not start with a `document_start`");

    ++iter;
    auto out = extract_single(*&iter, last);

    if (iter == last)
        throw std::invalid_argument("Extracting value into a JSON tree ended early");

    ++iter;
    if (iter == last)
        throw std::invalid_argument("Extracting value into a JSON tree ended early");

    auto last_node = *iter;
    if (last_node.type() != ast_node_type::document_end)
        throw std::invalid_argument(
            std::string("AST index does not end with `document_end`: ") + to_string(last_node.type()));

    return out;
}

value parse_index::extract_tree() const
{
    return extract_tree(parse_options::create_default());
}

value parse(const string_view& input, const parse_options& options)
{
    auto ast = parse_index::parse(input);
    ast.validate();
    return ast.extract_tree(options);
}

}

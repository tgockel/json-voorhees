/// \file
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/serialization/extract.hpp>
#include <jsonv/demangle.hpp>
#include <jsonv/value.hpp>

#include <sstream>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extraction_error::problem                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extraction_error::problem::problem(jsonv::path path, std::string message, std::exception_ptr cause) noexcept :
        _path(std::move(path)),
        _message(std::move(message)),
        _cause(std::move(cause))
{
    if (_message.empty())
        _message = "Unknown problem";
}

extraction_error::problem::problem(jsonv::path path, std::string message) noexcept :
        problem(std::move(path), std::move(message), nullptr)
{ }

extraction_error::problem::problem(jsonv::path path, std::exception_ptr cause) noexcept :
        problem(std::move(path),
                [&]() -> std::string
                {
                    try
                    {
                        std::rethrow_exception(cause);
                    }
                    catch (const std::exception& ex)
                    {
                        return ex.what();
                    }
                    catch (...)
                    {
                        std::ostringstream os;
                        os << "Exception with type " << current_exception_type_name();
                        return std::move(os).str();
                    }
                }(),
                cause
               )
{ }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extraction_error                                                                                                   //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string make_extraction_error_errmsg(const extraction_error::problem_list& problems)
{
    std::ostringstream os;

    auto write_problem =
        [&](const extraction_error::problem& problem)
        {
            if (!problem.path().empty())
                os << " at " << problem.path() << ": ";

            os << problem.message();
        };

    if (problems.size() == 0U)
    {
        os << "Extraction error with unspecified problem";
    }
    else if (problems.size() == 1U)
    {
        os << "Extraction error";
        write_problem(problems[0]);
    }
    else if (problems.size() > 1U)
    {
        os << problems.size() << " extraction errors:";

        for (const auto& problem : problems)
        {
            os << std::endl;
            os << " -";
            write_problem(problem);
        }
    }

    return std::move(os).str();
}

extraction_error::extraction_error(problem_list problems) noexcept :
        std::runtime_error(make_extraction_error_errmsg(problems)),
        _problems(std::move(problems))
{ }

template <typename... TArgs>
extraction_error::extraction_error(std::in_place_t, TArgs&&... args) noexcept :
        extraction_error(problem_list({ problem(std::forward<TArgs>(args)...) }))
{ }

extraction_error::extraction_error(jsonv::path path, std::string message, std::exception_ptr cause) noexcept :
        extraction_error(std::in_place, std::move(path), std::move(message), std::move(cause))
{ }

extraction_error::extraction_error(jsonv::path path, std::string message) noexcept :
        extraction_error(std::in_place, std::move(path), std::move(message))
{ }

extraction_error::extraction_error(jsonv::path path, std::exception_ptr cause) noexcept :
        extraction_error(std::in_place, std::move(path), std::move(cause))
{ }

extraction_error::~extraction_error() noexcept = default;

const path& extraction_error::path() const noexcept
{
    if (_problems.empty()) JSONV_UNLIKELY
    {
        static const jsonv::path empty_path;
        return empty_path;
    }
    else
    {
        return _problems[0].path();
    }
}

const std::exception_ptr& extraction_error::nested_ptr() const noexcept
{
    if (_problems.empty()) JSONV_UNLIKELY
    {
        static const std::exception_ptr empty_ex;
        return empty_ex;
    }
    else
    {
        return _problems[0].nested_ptr();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// context                                                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

context::context(jsonv::formats                  formats,
                 const std::optional<jsonv::version>& version,
                 const void*                     user_data
                ) :
        _formats(std::move(formats)),
        _version(version),
        _user_data(user_data)
{ }

context::context() :
        context(formats::global())
{ }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extraction_context                                                                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extraction_context::extraction_context(jsonv::formats                  formats,
                                       const extract_options&          options,
                                       const std::optional<jsonv::version>& version,
                                       const void*                     user_data
                                      ) :
        context(std::move(formats), version, user_data),
        _options(options)
{ }

extraction_context::extraction_context(jsonv::formats                  formats,
                                       const std::optional<jsonv::version>& version,
                                       const void*                     user_data
                                      ) :
        extraction_context(std::move(formats), extract_options::create_default(), version, user_data)
{ }

extraction_context::extraction_context() :
        extraction_context(formats::global())
{ }

extraction_context::~extraction_context() noexcept = default;

std::expected<void, ast_node_type> extraction_context::expect(reader& from, ast_node_type type)
{
    auto res = from.expect(type);
    if (!res)
    {
        std::ostringstream ss;
        ss << "Read node of type " << res.error() << " when expecting " << type;
        (void) problem(from.current_path(), std::move(ss).str());
        return std::unexpected(res.error());
    }
    else
    {
        return {};
    }
}

std::expected<void, ast_node_type>
extraction_context::expect(reader& from, std::initializer_list<ast_node_type> types)
{
    auto res = from.expect(types);
    if (!res)
    {
        std::ostringstream ss;
        ss << "Read node of type " << res.error() << " when expecting one of ";
        bool first = true;
        for (const auto& type : types)
        {
            if (!std::exchange(first, false))
                ss << ", ";
            ss << type;
        }
        (void) problem(from.current_path(), std::move(ss).str());
        return std::unexpected(res.error());
    }
    else
    {
        return {};
    }
}

std::expected<void, ast_node_type>
extraction_context::extract(const std::type_info& type, reader& from, void* into)
{
    try
    {
        return formats().get_extractor(type).extract(*this, from, into);
    }
    catch (const extraction_error& ex)
    {
        // Forward the inner exception's problem list so the path/message survive the boundary, but suppress the
        // implicit throw that `problem()` would normally do for fail-immediately mode -- we are already collapsing the
        // failure onto the std::expected error channel.
        const auto saved_mode = _options.failure_mode();
        _options.failure_mode(extract_options::on_error::collect_all);
        try
        {
            for (const auto& p : ex.problems())
                (void) problem(p);
        }
        catch (...)
        {
            _options.failure_mode(saved_mode);
            throw;
        }
        _options.failure_mode(saved_mode);
        return std::unexpected(ast_node_type::error);
    }
    catch (const std::exception& ex)
    {
        return problem(from.current_path(), ex.what(), std::current_exception());
    }
    catch (...)
    {
        return problem(from.current_path(),
                       std::string("Exception with type ") + current_exception_type_name(),
                       std::current_exception()
                      );
    }
}

void extraction_context::on_problem()
{
    if (   _options.failure_mode() == extract_options::on_error::fail_immediately
        || _problems.size() >= _options.max_failures()
       )
    {
        throw extraction_error(_problems);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extractor                                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extractor::~extractor() noexcept = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// read_value                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static value read_value_impl(reader& from);

static std::string read_key(const ast_node& node)
{
    switch (node.type())
    {
    case ast_node_type::key_canonical:
        return std::string(node.as<ast_node::key_canonical>().value());
    case ast_node_type::key_escaped:
        return node.as<ast_node::key_escaped>().value();
    default:
        throw extraction_error(jsonv::path(), "Expected an object key");
    }
}

static value read_object(reader& from)
{
    value out = object();
    while (from.next_token())
    {
        const auto& node = from.current();
        if (node.type() == ast_node_type::object_end)
            return out;

        std::string key = read_key(node);
        if (!from.next_token())
            throw extraction_error(from.current_path(), "Unexpected EOF after object key");

        out.insert({ std::move(key), read_value_impl(from) });
    }
    throw extraction_error(from.current_path(), "Unterminated object");
}

static value read_array(reader& from)
{
    value out = array();
    while (from.next_token())
    {
        const auto& node = from.current();
        if (node.type() == ast_node_type::array_end)
            return out;
        out.push_back(read_value_impl(from));
    }
    throw extraction_error(from.current_path(), "Unterminated array");
}

static value read_value_impl(reader& from)
{
    const auto& node = from.current();
    switch (node.type())
    {
    case ast_node_type::object_begin:
        return read_object(from);
    case ast_node_type::array_begin:
        return read_array(from);
    case ast_node_type::string_canonical:
        return value(std::string(node.as<ast_node::string_canonical>().value()));
    case ast_node_type::string_escaped:
        return value(node.as<ast_node::string_escaped>().value());
    case ast_node_type::literal_true:
        return value(true);
    case ast_node_type::literal_false:
        return value(false);
    case ast_node_type::literal_null:
        return null;
    case ast_node_type::integer:
        return value(node.as<ast_node::integer>().value());
    case ast_node_type::decimal:
        return value(node.as<ast_node::decimal>().value());
    case ast_node_type::document_start:
    case ast_node_type::document_end:
    case ast_node_type::object_end:
    case ast_node_type::array_end:
    case ast_node_type::key_canonical:
    case ast_node_type::key_escaped:
    case ast_node_type::error:
        break;
    }
    std::ostringstream ss;
    ss << "Unexpected token of type " << node.type() << " while reading value";
    throw extraction_error(from.current_path(), std::move(ss).str());
}

value read_value(reader& from)
{
    if (from.good() && from.current().type() == ast_node_type::document_start)
    {
        (void) from.next_token();
    }
    return read_value_impl(from);
}

}

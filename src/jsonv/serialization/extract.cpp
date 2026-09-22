/// \file
/// Extraction of C++ types from JSON values.
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
// extractor                                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extractor::~extractor() noexcept = default;

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
    if (_problems.empty())
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
    if (_problems.empty())
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
// extract_options                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extract_options::extract_options() noexcept = default;

extract_options::~extract_options() noexcept = default;

extract_options extract_options::create_default()
{
    return extract_options();
}

extract_options& extract_options::failure_mode(on_error mode)
{
    _failure_mode = mode;
    return *this;
}

extract_options& extract_options::max_failures(size_type limit)
{
    _max_failures = limit;
    return *this;
}

extract_options& extract_options::on_duplicate_key(duplicate_key_action action)
{
    _on_duplicate_key = action;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extraction_context                                                                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extraction_context::extraction_context(jsonv::formats                fmt,
                                       std::optional<jsonv::version> ver,
                                       jsonv::path                   p,
                                       const void*                   userdata
                                      ) :
        context(std::move(fmt), std::move(ver), userdata),
        _path(std::move(p))
{ }

extraction_context::extraction_context() :
        context()
{ }

extraction_context::~extraction_context() noexcept = default;

void extraction_context::extract(const std::type_info& type, const value& from, void* into) const
{
    try
    {
        formats().extract(type, from, into, *this);
    }
    catch (const extraction_error&)
    {
        throw;
    }
    catch (const std::exception& ex)
    {
        throw extraction_error(path(), ex.what(), std::current_exception());
    }
    catch (...)
    {
        throw extraction_error(path(),
                               std::string("Exception with type ") + current_exception_type_name(),
                               std::current_exception()
                              );
    }
}

void extraction_context::extract_sub(const std::type_info& type,
                                     const value&          from,
                                     jsonv::path           subpath,
                                     void*                 into
                                    ) const
{
    extraction_context sub(*this);
    sub._path += subpath;
    try
    {
        return sub.extract(type, from.at_path(subpath), into);
    }
    catch (const extraction_error&)
    {
        throw;
    }
    catch (const std::exception& ex)
    {
        throw extraction_error(sub.path(), ex.what(), std::current_exception());
    }
    catch (...)
    {
        throw extraction_error(sub.path(),
                               std::string("Exception with type ") + current_exception_type_name(),
                               std::current_exception()
                              );
    }
}

}

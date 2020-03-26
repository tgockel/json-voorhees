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
    if (_problems.empty())
    {
        JSONV_UNLIKELY

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
        JSONV_UNLIKELY

        static const std::exception_ptr empty_ex;
        return empty_ex;
    }
    else
    {
        return _problems[0].nested_ptr();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extraction_context                                                                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extraction_context::extraction_context(jsonv::formats        fmt,
                                       const jsonv::version& ver,
                                       const void*           userdata
                                      ) :
        context_base(std::move(fmt), ver, userdata),
        _path(std::move(p))
{ }

extraction_context::extraction_context() :
        context_base()
{ }

extraction_context::~extraction_context() noexcept = default;

result<void, void> extraction_context::expect(reader& from, ast_node_type type)
{
    auto res = from.expect(type);
    if (res.is_error())
    {
        std::ostringstream ss;
        ss << "Read node of type " << res.error() << " when expecting " << type;
        return problem(from.current_path(), std::move(ss).str());
    }
    else
    {
        return ok{};
    }
}

result<void, void> extraction_context::expect(reader& from, std::initializer_list<ast_node_type> types)
{
    auto res = from.expect(types);
    if (res.is_error())
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
        return problem(from.current_path(), std::move(ss).str());
    }
    else
    {
        return ok{};
    }
}

result<void, void> extraction_context::extract(const std::type_info& type, reader& from, void* into)
{
    try
    {
        formats().get_extractor(type).extract(*this, from, into);
        return ok{};
    }
    catch (const extraction_error&)
    {
        return error{};
    }
    catch (const std::exception& ex)
    {
        return problem(path(), ex.what(), std::current_exception());
    }
    catch (...)
    {
        return problem(path(),
                       std::string("Exception with type ") + current_exception_type_name(),
                       std::current_exception()
                      );
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extractor                                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extractor::~extractor() noexcept = default;

}

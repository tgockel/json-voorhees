/// \file jsonv/serialization/extractor_construction.hpp
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/serialization/extract.hpp>

#include <exception>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An \c extractor for type \c T that has a constructor that accepts either a \c jsonv::reader or a \c jsonv::reader
/// and \c jsonv::extraction_context.
template <typename T>
class extractor_construction :
        public extractor
{
public:
    /// \see extractor::get_type
    virtual const std::type_info& get_type() const noexcept override
    {
        return typeid(T);
    }

    /// \see extractor::extract
    virtual
    result<void, void>
    extract(extraction_context& context,
            reader&             from,
            void*               into
           ) const override
    {
        auto problem_count_before = context.problems().size();
        try
        {
            extract_impl<T>(context, from, into);
            return ok{};
        }
        catch (const extraction_error& ex)
        {
            // If we still have the same number of problems in the list, the constructor threw without adding to the
            // problem list.
            if (problem_count_before == context.problems().size())
            {
                for (const auto& p : ex.problems())
                    context.problem(p);
            }
            return error{};
        }
        catch (...)
        {
            // If we still have the same number of problems in the list, the constructor threw without adding to the
            // problem list.
            if (problem_count_before == context.problems().size())
            {
                context.problem(from.current_path(), std::current_exception());
            }
            return error{};
        }
    }

protected:
    template <typename U>
    auto extract_impl(extraction_context& context,
                      reader&             from,
                      void*               into
                     ) const
            -> decltype(U(from, context), void())
    {
        new(into) U(from, context);
    }

    template <typename U, typename = void>
    auto extract_impl(extraction_context&,
                      reader&             from,
                      void*               into
                     ) const
            -> decltype(U(from), void())
    {
        new(into) U(from);
    }
};

/// \}

}

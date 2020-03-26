/// \file jsonv/serialization/optional_adapter.hpp
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
#include <jsonv/serialization.hpp>
#include <jsonv/serialization/extract.hpp>

namespace jsonv
{

/// An \c adapter is both an \c extractor and a \c serializer. It is made with the idea that for \e most types, you want
/// to both encode and decode JSON.
class JSONV_PUBLIC adapter :
        public extractor,
        public serializer
{
public:
    virtual ~adapter() noexcept;

    /// \see extractor::get_type
    /// \see serializer::get_type
    virtual const std::type_info& get_type() const noexcept = 0;
};

}

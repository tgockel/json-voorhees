/// \file
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

#include <string>
#include <utility>

#include "detail/cloneable.hpp"

namespace jsonv::detail
{

class JSONV_LOCAL string_impl final :
        public cloneable<string_impl>
{
public:
    /// Take \a text as the string. Taking it by value means a caller with a string to give away hands it over, and
    /// one building it from a view or a conversion builds it straight into the node, so the text is copied at most
    /// once on the way in.
    explicit string_impl(std::string text) noexcept :
            _string(std::move(text))
    { }

public:
    std::string _string;
};

}

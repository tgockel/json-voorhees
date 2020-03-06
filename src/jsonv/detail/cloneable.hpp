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

namespace jsonv::detail
{

/// Creates a \c clone function which uses the copy constructor of the leaf type to create a pointer clone.
template <typename T>
struct cloneable
{
    T* clone() const
    {
        return new T(*static_cast<const T*>(this));
    }
};

}

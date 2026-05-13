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

#include "detail/cloneable.hpp"

namespace jsonv::detail
{

class JSONV_LOCAL string_impl final :
        public cloneable<string_impl>
{
public:
    std::string _string;
};

}

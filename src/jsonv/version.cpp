/// \file
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/version.hpp>

#include <ostream>

namespace jsonv
{

std::ostream& operator<<(std::ostream& os, const version& self)
{
    return os << self.major << '.' << self.minor;
}

}

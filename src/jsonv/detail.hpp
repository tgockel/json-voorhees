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

#include <jsonv/value.hpp>
#include <jsonv/string_view.hpp>

namespace jsonv
{

const char* kind_desc(kind type);
bool kind_valid(kind k);
void check_type(kind expected, kind actual);
void check_type(std::initializer_list<kind> expected, kind actual);
std::ostream& stream_escaped_string(std::ostream& stream, string_view str, bool require_ascii);

}

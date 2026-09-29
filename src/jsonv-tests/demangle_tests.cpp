/** \file
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"

#include <jsonv/demangle.hpp>
#include <jsonv/detail/scope_exit.hpp>

#include <iostream>
#include <typeinfo>

namespace jsonv_test
{

TEST(demangle_types)
{
    std::cout << jsonv::demangle(typeid(std::string_view).name());
    (void) jsonv::demangle("_ZN20garbage");
}

// The view stops short of the buffer's last character, so there is no terminator where it ends. A demangler which reads
// to the terminator sees the trailing `x` and gives up on a name it would otherwise have understood.
TEST(demangle_unterminated_view)
{
    std::string      buffer = std::string(typeid(int).name()) + "x";
    std::string_view source(buffer.data(), buffer.size() - 1U);
    ensure_eq(jsonv::demangle(typeid(int).name()), jsonv::demangle(source));
}

TEST(demangle_set_reset)
{
    jsonv::set_demangle_function(nullptr);
    auto cleanup = jsonv::detail::on_scope_exit(jsonv::reset_demangle_function);
    (void) jsonv::demangle(typeid(int).name());
}

}

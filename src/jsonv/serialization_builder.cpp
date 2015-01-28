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
#include <jsonv/serialization_builder.hpp>

namespace jsonv
{
namespace detail
{

formats_builder_dsl::operator formats() const
{
    return *owner;
}

formats_builder& formats_builder_dsl::register_adapter(std::shared_ptr<const adapter> p)
{
    return owner->register_adapter(std::move(p));
}

}

}

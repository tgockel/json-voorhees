/// \file
/// A human-readable name for an \c ast_node_type, shared by everything which reports an extraction problem.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/ast.hpp>

#include <string_view>

namespace jsonv
{

/// A human-readable name for \a type.
///
/// \c operator<<(std::ostream&, ast_node_type) writes the single-character tape representation, which is what dumping
/// a token stream in a test wants and is not what someone reading \c extraction_error::what() wants. The two string
/// node types share a name, as do the two key types, since the distinction between them is about how the source spelt
/// a string and not about what was found.
JSONV_NODISCARD JSONV_LOCAL std::string_view describe(ast_node_type type);

}

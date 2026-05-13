/// \file jsonv/forward.hpp
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <cstdint>

namespace jsonv
{

class adapter;
template <typename T> class adapter_builder;
enum class ast_error : std::uint64_t;
class ast_node;
enum class ast_node_type : std::uint8_t;
class encoder;
template <typename TError> class error;
class extractor;
class extraction_context;
class extract_options;
class formats;
class formats_builder;
enum class kind : unsigned char;
class kind_error;
template <typename T, typename TMember> class member_adapter_builder;
template <typename TValue> class ok;
class parse_error;
class parse_index;
class parse_options;
class path;
class path_element;
enum class path_element_kind : unsigned char;
template <typename TPointer> class polymorphic_adapter_builder;
class reader;
template <typename TValue, typename TError> class result;
class serializer;
class serialization_context;
class value;
struct version;

}

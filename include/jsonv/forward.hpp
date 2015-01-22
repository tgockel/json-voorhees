/** \file jsonv/forward.hpp
 *  
 *  Copyright (c) 2012-2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_FORWARD_HPP_INCLUDED__
#define __JSONV_FORWARD_HPP_INCLUDED__

namespace jsonv
{

class adapter;
class encoder;
class extractor;
class extraction_context;
class formats;
enum class kind : unsigned char;
class kind_error;
class parse_error;
class parse_options;
class path;
class path_element;
enum class path_element_kind : unsigned char;
class serializer;
class serialization_context;
class tokenizer;
enum class token_kind : unsigned int;
class value;
struct version;

}

#endif/*__JSONV_FORWARD_HPP_INCLUDED__*/

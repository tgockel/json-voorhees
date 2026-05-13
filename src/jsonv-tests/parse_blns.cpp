/** \file
 *  Parse the [Big List of Naughty Strings](https://github.com/minimaxir/big-list-of-naughty-strings) JSON file.
 *
 *  Copyright (c) 2018 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
 *
 *  \paragraph
 *  The "Big List of Naughty Strings" is used under the conditions of the MIT License.
 *
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2015 Max Woolf
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
**/
#include "filesystem_util.hpp"
#include "test.hpp"

#include <jsonv/parse.hpp>

#include <fstream>

namespace jsonv_test
{

TEST(parse_naughty_strings)
{
    std::ifstream src_file(test_path("blns.json"));
    auto val = jsonv::parse(src_file);

    // Unclear exactly how to test this...so we'll just make sure that we parse an array and that all elements of the
    // array are strings.
    ensure(val.is_array());
    for (const auto& sub : val.as_array())
    {
        ensure(sub.is_string());
    }
}

}

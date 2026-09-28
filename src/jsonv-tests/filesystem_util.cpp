/** \file
 *  Utility functions for interacting with the filesystem.
 *
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "filesystem_util.hpp"

#include <filesystem>

#ifndef JSONV_TEST_DATA_DIR
#   define JSONV_TEST_DATA_DIR "."
#endif

namespace jsonv_test
{

namespace fs = std::filesystem;

std::string test_path(const std::string& path)
{
    return std::string(JSONV_TEST_DATA_DIR) + "/" + path;
}

std::string filename(std::string path)
{
    return fs::path(path).filename().string();
}

void recursive_directory_for_each(const std::string&                             root_path_name,
                                  const std::string&                             extension_filter,
                                  const std::function<void (const std::string&)> action
                                 )
{
    for (const auto& entry : fs::recursive_directory_iterator(root_path_name))
    {
        if (entry.is_regular_file() && (extension_filter.empty() || entry.path().extension() == extension_filter))
            action(entry.path().string());
    }
}

}

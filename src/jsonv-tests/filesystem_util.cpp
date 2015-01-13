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

#include <boost/filesystem.hpp>

namespace jsonv_test
{

namespace fs = boost::filesystem;

std::string filename(std::string path)
{
    return fs::path(path).filename().string();
}

void recursive_directory_for_each(const std::string&                             root_path_name,
                                  const std::string&                             extension_filter,
                                  const std::function<void (const std::string&)> action
                                 )
{
    fs::path rootpath(root_path_name);
    for (fs::directory_iterator iter(rootpath); iter != fs::directory_iterator(); ++iter)
    {
        fs::path p = *iter;
        if (extension_filter.empty() || p.extension() == extension_filter)
            action(p.string());
    }
}

}

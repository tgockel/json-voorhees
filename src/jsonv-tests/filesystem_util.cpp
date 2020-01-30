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

#include <jsonv/detail/scope_exit.hpp>

#ifndef JSONV_TEST_DATA_DIR
#   define JSONV_TEST_DATA_DIR "."
#endif

namespace jsonv_test
{

std::string test_path(const std::string& path)
{
    return std::string(JSONV_TEST_DATA_DIR) + "/" + path;
}

}

// TODO: libstdc++ requires explicit linking to libstdc++fs, but CMake does not have great cross-platform support for
// linking when it is needed. Since a POSIX fallback exists anyway, we will always just use it on non-Windows platforms.
// The `0` should be taken out when `<filesystem>` support actually becomes universal.
#if 0 && __has_include(<filesystem>)

#include <filesystem>

namespace jsonv_test
{

namespace fs = std::filesystem;

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

#elif defined _MSC_VER

#include <Windows.h>

namespace jsonv_test
{

std::string filename(std::string path)
{
    auto pos = path.find_last_of('\\');
    return path.substr(pos + 1);
}

void recursive_directory_for_each(const std::string&                             root_path_name,
                                  const std::string&                             extension_filter,
                                  const std::function<void (const std::string&)> action
                                 )
{
    WIN32_FIND_DATAA found_file;
    HANDLE search_handle = NULL;

    std::string filter = root_path_name + "\\*" + extension_filter;

    if ((search_handle = FindFirstFileA(filter.c_str(), &found_file)) == INVALID_HANDLE_VALUE)
        return;

    auto clean_handle = jsonv::detail::on_scope_exit([&search_handle] { FindClose(search_handle); });

    do
    {
        // skip "." and ".."
        if (strcmp(found_file.cFileName, ".") == 0
            || strcmp(found_file.cFileName, "..") == 0
            )
        {
            continue;
        }
        else
        {
            std::string path = root_path_name + "\\" + found_file.cFileName;

            if (found_file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                recursive_directory_for_each(path, extension_filter, action);
            else
                action(path);
        }
    } while (FindNextFileA(search_handle, &found_file));
}

}

#else /* No <filesystem> and not Windows, so assume POSIX API...remove when <filesystem> is everywhere */

#include <cstring>

#include <dirent.h>
#include <ftw.h>
#include <unistd.h>

namespace jsonv_test
{

std::string filename(std::string path)
{
    auto pos = path.find_last_of('/');
    return path.substr(pos + 1);
}

void recursive_directory_for_each(const std::string&                             root_path_name,
                                  const std::string&                             extension_filter,
                                  const std::function<void (const std::string&)> action
                                 )
{
    if (auto dirp = ::opendir(root_path_name.c_str()))
    {
        auto close_dirp = jsonv::detail::on_scope_exit([&] { ::closedir(dirp); });

        while (auto entp = ::readdir(dirp))
        {
            // skip "." and ".."
            if (std::strcmp(entp->d_name, ".") == 0
                || std::strcmp(entp->d_name, "..") == 0
                )
            {
                continue;
            }

            std::string sub = root_path_name + "/" + entp->d_name;

            if ((entp->d_type & DT_DIR) == DT_DIR)
            {
                recursive_directory_for_each(sub, extension_filter, action);
            }
            else
            {
                if (extension_filter.empty() || sub.rfind(extension_filter) != std::string::npos)
                    action(sub);
            }
        }
    }
}

}

#endif

/** \file
 *
 *  Copyright (c) 2018 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/algorithm.hpp>
#include <jsonv/value.hpp>

namespace jsonv
{

diff_result diff(value left, value right)
{
    diff_result result;
    if (left == right)
    {
        result.same = std::move(left);
    }
    else if (left.kind() != right.kind())
    {
        result.left  = std::move(left);
        result.right = std::move(right);
    }
    else switch (left.kind())
    {
    case kind::boolean:
    case kind::decimal:
    case kind::integer:
    case kind::null:
    case kind::string:
        result.left = std::move(left);
        result.right = std::move(right);
        break;
    case kind::array:
        result.same = array();
        result.left = array();
        result.right = array();
        for (value::size_type idx = 0; idx < std::min(left.size(), right.size()); ++idx)
        {
            diff_result subresult = diff(std::move(left.at(idx)), std::move(right.at(idx)));
            result.same.push_back(std::move(subresult.same));
            result.left.push_back(std::move(subresult.left));
            result.right.push_back(std::move(subresult.right));
        }

        if (left.size() > right.size())
            result.left.insert(result.left.end_array(),
                               std::make_move_iterator(left.begin_array() + right.size()),
                               std::make_move_iterator(left.end_array())
                              );
        else if (left.size() < right.size())
            result.right.insert(result.right.end_array(),
                                std::make_move_iterator(right.begin_array() + left.size()),
                                std::make_move_iterator(right.end_array())
                               );
        break;
    case kind::object:
        result.same = object();
        result.left = object();
        result.right = object();
        for (value::object_iterator liter = left.begin_object();
             liter != left.end_object();
             liter = left.erase(liter)
            )
        {
            auto riter = right.find(liter->first);
            if (riter == right.end_object())
            {
                result.left.insert({ liter->first, std::move(liter->second) });
            }
            else if (liter->second == riter->second)
            {
                result.same[liter->first] = std::move(liter->second);
                right.erase(riter);
            }
            else
            {
                diff_result subresult = diff(std::move(liter->second), std::move(riter->second));
                result.left[liter->first] = std::move(subresult.left);
                result.right[liter->first] = std::move(subresult.right);
                right.erase(riter);
            }
        }

        for (value::object_iterator riter = right.begin_object();
             riter != right.end_object();
             riter = right.erase(riter)
            )
        {
            result.right.insert({ riter->first, std::move(riter->second) });
        }
        break;
    }
    return result;
}

}

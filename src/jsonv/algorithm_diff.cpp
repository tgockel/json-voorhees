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
        // Both objects iterate in key order, so walking them side by side visits every key once, and every insert
        // into the results lands at the end, where it is hinted.
        {
            auto liter = left.begin_object();
            auto riter = right.begin_object();
            while (liter != left.end_object() || riter != right.end_object())
            {
                if (riter == right.end_object() || (liter != left.end_object() && liter->first < riter->first))
                {
                    result.left.insert(result.left.end_object(), { liter->first, std::move(liter->second) });
                    ++liter;
                }
                else if (liter == left.end_object() || riter->first < liter->first)
                {
                    result.right.insert(result.right.end_object(), { riter->first, std::move(riter->second) });
                    ++riter;
                }
                else
                {
                    if (liter->second == riter->second)
                    {
                        result.same.insert(result.same.end_object(), { liter->first, std::move(liter->second) });
                    }
                    else
                    {
                        diff_result subresult = diff(std::move(liter->second), std::move(riter->second));
                        result.left.insert(result.left.end_object(), { liter->first, std::move(subresult.left) });
                        result.right.insert(result.right.end_object(), { liter->first, std::move(subresult.right) });
                    }
                    ++liter;
                    ++riter;
                }
            }
        }
        break;
    }
    return result;
}

}

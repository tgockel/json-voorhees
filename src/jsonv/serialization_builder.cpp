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
#include <jsonv/demangle.hpp>

#include <deque>
#include <sstream>
#include <tuple>

namespace jsonv
{

formats_builder::formats_builder() :
            _referenced_types()
{ }

formats_builder& formats_builder::reference_type(std::type_index type)
{
    _referenced_types[type];
    return *this;
}

formats_builder& formats_builder::reference_type(std::type_index type, std::type_index from)
{
    _referenced_types[type].insert(from);
    return *this;
}

void formats_builder::check_references_impl(const formats& searching, const std::string& name)
{
    std::deque<std::tuple<std::type_index, bool, bool>> failed_types;
    for (const auto& pair : _referenced_types)
    {
        const std::type_index& type = pair.first;
        
        bool has_extractor  = [&] { try { searching.get_extractor(type);  return true; } catch (const no_extractor&)  { return false; } }();
        bool has_serializer = [&] { try { searching.get_serializer(type); return true; } catch (const no_serializer&) { return false; } }();
        
        if (!has_extractor || !has_serializer)
            failed_types.emplace_back(type, has_extractor, has_serializer);
    }
    
    if (!failed_types.empty())
    {
        std::ostringstream os;
        if (!name.empty())
            os << name << ": ";
        bool singular = failed_types.size() == 1;
        os << "There " << (singular ? "is " : "are ") << failed_types.size() << " type" << (singular ? "" : "s")
           << " referenced that the formats do not know how to serialize: ";
        for (const auto& failed_info : failed_types)
        {
            std::type_index type = std::get<0>(failed_info);
            os << std::endl << " - " << demangle(type.name());
            const auto& referencing_types = _referenced_types.at(type);
            if (!referencing_types.empty())
            {
                os << " (referenced by: ";
                bool first = true;
                for (const std::type_index& referencing_type : referencing_types)
                {
                    if (first)
                        first = false;
                    else
                        os << ", ";
                    os << demangle(referencing_type.name());
                }
                os << ")";
            }
        }
        throw std::logic_error(os.str());
    }
}

formats_builder& formats_builder::check_references(const formats& other, const std::string& name)
{
    check_references_impl(formats::compose({ _formats, other }), name);
    return *this;
}

formats_builder& formats_builder::check_references(const formats::list& others, const std::string& name)
{
    return check_references(formats::compose(others), name);
}

formats_builder& formats_builder::check_references(const std::string& name)
{
    check_references_impl(*this, name);
    return *this;
}

formats formats_builder::compose_checked(formats other, const std::string& name)
{
    formats searching = formats::compose({ *this, other });
    check_references_impl(searching, name);
    return searching;
}

formats formats_builder::compose_checked(const formats::list& others, const std::string& name)
{
    formats::list all;
    all.reserve(others.size() + 1);
    all.emplace_back(*this);
    std::copy(others.begin(), others.end(), std::back_inserter(all));

    formats searching = formats::compose(all);
    check_references_impl(searching, name);
    return searching;
}

formats_builder& formats_builder::on_duplicate_type(duplicate_type_action action) noexcept
{
    _duplicate_type_action = action;
    return *this;
}

namespace detail
{

formats_builder_dsl::operator formats() const
{
    return *owner;
}

formats_builder& formats_builder_dsl::reference_type(std::type_index typ)
{
    return owner->reference_type(typ);
}

formats_builder& formats_builder_dsl::reference_type(std::type_index type, std::type_index from)
{
    return owner->reference_type(type, from);
}

formats_builder& formats_builder_dsl::register_adapter(const adapter* p)
{
    return owner->register_adapter(p);
}

formats_builder& formats_builder_dsl::register_adapter(std::shared_ptr<const adapter> p)
{
    return owner->register_adapter(std::move(p));
}

formats_builder& formats_builder_dsl::check_references(const formats& other, const std::string& name)
{
    return owner->check_references(other, name);
}

formats_builder& formats_builder_dsl::check_references(const formats::list& others, const std::string& name)
{
    return owner->check_references(others, name);
}

formats_builder& formats_builder_dsl::check_references(const std::string& name)
{
    return owner->check_references(name);
}

formats formats_builder_dsl::compose_checked(formats other, const std::string& name)
{
    return owner->compose_checked(std::move(other), name);
}

formats formats_builder_dsl::compose_checked(std::vector<formats> others, const std::string& name)
{
    return owner->compose_checked(std::move(others), name);
}

formats_builder& formats_builder_dsl::on_duplicate_type(duplicate_type_action action) noexcept
{
    return owner->on_duplicate_type(action);
}

}

void throw_extra_keys_extraction_error(const extraction_context&    context,
                                       const value&,
                                       const std::set<std::string>& extra_keys
                                      )
{
    std::ostringstream os;
    bool plural = extra_keys.size() != 1;
    os << "Found extra key" << (plural ? "s" : "") << " in value: ";
    bool first = true;
    for (const std::string& key : extra_keys)
    {
        if (first)
            first = false;
        else
            os << ", ";
        os << key;
    }
    throw extraction_error(context, os.str());
}

}

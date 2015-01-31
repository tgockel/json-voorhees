/** \file jsonv/serialization_builder.hpp
 *  DSL for building \c formats.
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
 *  
 *  \page serialization_builder_dsl Serialization Builder DSL
 *  
 *  Most applications tend to have a lot of structure types. While it is possible to write an \c extractor and
 *  \c serializer (or \c adapter) for each type, this can get a little bit tedious. Beyond that, it is very difficult to
 *  look at the contents of adapter code and discover what the JSON might actually look like. The builder DSL is meant
 *  to solve these issues by providing a convenient way to describe conversion operations for your C++ types.
 *  
 *  At the end of the day, the goal is to take some C++ structures like this:
 *  
 *  \code
 *  struct person
 *  {
 *      std::string first_name;
 *      std::string last_name;
 *      int         age;
 *      std::string role;
 *  };
 *  
 *  struct company
 *  {
 *      std::string         name;
 *      bool                certified;
 *      std::vector<person> employees;
 *      std::list<person>   candidates;
 *  };
 *  \endcode
 *  
 *  ...and easily convert it to an from a JSON representation that looks like this:
 *  
 *  \code
 *  {
 *      "name": "Paul's Construction",
 *      "certified": false,
 *      "employees": [
 *          {
 *              "first_name": "Bob",
 *              "last_name":  "Builder",
 *              "age":        29
 *          },
 *          {
 *              "first_name": "James",
 *              "last_name":  "Johnson",
 *              "age":        38,
 *              "role":       "Foreman"
 *          }
 *      ],
 *      "candidates": [
 *          {
 *              "firstname": "Adam",
 *              "lastname":  "Ant"
 *          }
 *      ]
 *  }
 *  \endcode
 *  
 *  To define a \c formats for this \c person type using the serialization builder DSL, you would say:
 *  
 *  \code
 *  jsonv::formats fmts =
 *      jsonv::formats_builder()
 *          .type<person>()
 *              .member("first_name", &person::first_name)
 *                  .alternate_name("firstname")
 *              .member("last_name",  &person::last_name)
 *                  .alternate_name("lastname")
 *              .member("age",        &person::age)
 *                  .until({ 6,1 })
 *                  .default_value(21)
 *                  .default_on_null()
 *                  .check_input([] (int value) { if (value < 0) throw std::logic_error("Age must be positive."); })
 *              .member("role",       &person::role)
 *                  .since({ 2,0 })
 *                  .default_value("Builder")
 *          .type<company>()
 *              .member("name",       &company::name)
 *              .member("certified",  &company::certified)
 *              .member("employees",  &company::employees)
 *              .member("candidates", &company::candidates)
 *          .register_sequence_containers<company, std::vector, std::list>()
 *      ;
 *  \endcode
 * 
 *  \section Reference
 *  
 *  The DSL is made up of three major parts:
 *  
 *   1. \e formats -- modifies a \c jsonv::formats object by adding new type adapters to it
 *   2. \e type -- modifies the behavior of a \c jsonv::adapter by adding new members to it
 *   3. \e member -- modifies an individual member inside of a specific type
 *  
 *  Each successive function call transforms your context. \e Narrowing calls make your context more specific; for
 *  example, calling \c type from a \e formats context allows you to modify a specific type. \e Widening calls make the
 *  context less specific and are always available; for example, when in the \e member context, you can still call
 *  \c type from the \e formats context to specify a new type.
 *  
 *  \dot
 *  digraph serialization_builder_dsl {
 *    formats  [label="formats"]
 *    type     [label="type"]
 *    member   [label="member"]
 *    
 *    formats -> formats
 *    formats -> type
 *    type    -> formats
 *    type    -> type
 *    type    -> member
 *    member  -> formats
 *    member  -> type
 *    member  -> member
 *  }
 *  \enddot
 *  
 *  \subsection serialization_builder_dsl_ref_formats Formats Context
 *  
 *  Commands in this section modify the behavior of the underlying \c jsonv::formats object.
 *  
 *  \subsubsection serialization_builder_dsl_ref_formats_level Level
 *  
 *  \paragraph serialization_builder_dsl_ref_formats_level_check_references check_references
 *  
 *  TODO
 *  
 *  \paragraph serialization_builder_dsl_ref_formats_level_reference_type reference_type
 *  
 *  TODO
 *  
 *  \paragraph serialization_builder_dsl_ref_formats_level_register_adapter register_adapter
 *  
 *  TODO
 * 
 *  \paragraph serialization_builder_dsl_ref_formats_level_register_sequence_container register_sequence_container&lt;TContainer&gt;
 *  
 *  TODO
 * 
 *  \paragraph serialization_builder_dsl_ref_formats_level_register_sequence_containers register_sequence_containers&lt;T, template... &lt;T, ...&gt;&gt;
 *  
 *  TODO
 *  
 *  \subsubsection serialization_builder_dsl_ref_formats_narrowing Narrowing
 *  
 *  \paragraph serialization_builder_dsl_ref_formats_narrowing_type type&lt;T&gt;
 *  
 *  TODO
 *  
 *  \subsection serialization_builder_dsl_ref_type Type Context
 *  
 *  Commands in this section modify the behavior of the \c jsonv::adapter for a particular type.
 *  
 *  \subsubsection serialization_builder_dsl_ref_type_level Level
 *  
 *  \subsubsection serialization_builder_dsl_ref_type_narrowing Narrowing
 *  
 *  \paragraph serialization_builder_dsl_ref_type_narrowing_member member
 *  
 *  TODO
 *  
 *  \subsection serialization_builder_dsl_ref_member Member Context
 *  
 *  \subsubsection serialization_builder_dsl_ref_member_level Level
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_after after
 *  
 *  TODO
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_alternate_name alternate_name
 *  
 *  TODO
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_before before
 *  
 *  TODO
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_default_value default_value
 *  
 *  TODO
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_default_on_null default_on_null
 *  
 *  TODO
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_encode_if encode_if
 *  
 *  TODO
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_since since
 *  
 *  TODO
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_until until
 *  
 *  TODO
 *  
**/
#ifndef __JSONV_SERIALIZATION_BUILDER_HPP_INCLUDED__
#define __JSONV_SERIALIZATION_BUILDER_HPP_INCLUDED__

#include <jsonv/config.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization_util.hpp>

#include <deque>
#include <memory>
#include <set>
#include <type_traits>

namespace jsonv
{

class formats_builder;

namespace detail
{

template <typename T> class adapter_builder;
template <typename T, typename TMember> class member_adapter_builder;

class formats_builder_dsl
{
public:
    explicit formats_builder_dsl(formats_builder* owner) :
            owner(owner)
    { }
    
    template <typename T>
    detail::adapter_builder<T> type();
    
    formats_builder& register_adapter(std::shared_ptr<const adapter> p);
    
    formats_builder& reference_type(std::type_index typ);
    
    formats_builder& check_references(formats other, const std::string& name = "");
    
    operator formats() const;
    
protected:
    formats_builder* owner;
};

template <typename T>
class adapter_builder_dsl
{
public:
    explicit adapter_builder_dsl(adapter_builder<T>* owner) :
            owner(owner)
    { }
    
    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string name, TMember T::*selector);
    
protected:
    adapter_builder<T>* owner;
};

template <typename T>
class member_adapter
{
public:
    virtual ~member_adapter() noexcept
    { }
    
    virtual void mutate(const extraction_context& context, const value& from, T& out) const = 0;
    
    virtual void encode(const serialization_context& context, const T& from, value& out) const = 0;
};

template <typename T, typename TMember>
class member_adapter_impl :
        public member_adapter<T>
{
public:
    explicit member_adapter_impl(std::string name, TMember T::*selector) :
            _names({ std::move(name) }),
            _selector(selector)
    { }
    
    virtual void mutate(const extraction_context& context, const value& from, T& out) const override
    {
        value::const_object_iterator iter;
        for (const auto& name : _names)
            if ((iter = from.find(name)) != from.end_object())
                break;
        
        bool use_default = false;
        if (_default_value)
        {
            use_default = (iter == from.end_object())
                       || (_default_on_null && iter->second.kind() == kind::null);
        }
        
        if (use_default)
            (out.*_selector) = _default_value(context, from);
        else
            (out.*_selector) = context.extract_sub<TMember>(from, iter->first);
    }
    
    virtual void encode(const serialization_context& context, const T& from, value& out) const override
    {
        if (should_encode(context, from))
            out.insert({ _names.at(0), context.encode(from.*_selector) });
    }
    
    void add_encode_check(std::function<bool (const serialization_context&, const T&)> check)
    {
        if (_should_encode)
        {
            auto old_check = std::move(_should_encode);
            _should_encode = [check, old_check] (const serialization_context& context, const T& value)
                             {
                                return check(context, value) && old_check(context, value);
                             };
        }
        else
        {
            _should_encode = std::move(check);
        }
    }
    
private:
    bool should_encode(const serialization_context& context, const T& from) const
    {
        if (_should_encode)
            return _should_encode(context, from);
        else
            return true;
    }
    
private:
    std::vector<std::string>                                         _names;
    TMember T::*                                                     _selector;
    std::function<bool (const serialization_context&, const T&)>     _should_encode;
    std::function<TMember (const extraction_context&, const value&)> _default_value;
    bool                                                             _default_on_null = true;
};

template <typename T, typename TMember>
class member_adapter_builder :
        public formats_builder_dsl,
        public adapter_builder_dsl<T>
{
public:
    explicit member_adapter_builder(formats_builder*                 fmt_builder,
                                    adapter_builder<T>*              adapt_builder,
                                    member_adapter_impl<T, TMember>* adapter
                                   ) :
            formats_builder_dsl(fmt_builder),
            adapter_builder_dsl<T>(adapt_builder),
            _adapter(adapter)
    {
        reference_type(std::type_index(typeid(TMember)));
    }
    
    /** When extracting, also look for this \a name as a key. **/
    member_adapter_builder& alternate_name(std::string name)
    {
        _adapter->_names.emplace_back(std::move(name));
        return *this;
    }
    
    /** If the key for this member is not in the object when deserializing, call this function to create a value. If a
     *  \c default_value is not specified, the key is required.
    **/
    member_adapter_builder& default_value(std::function<TMember (const extraction_context&, const value&)> create)
    {
        _adapter->_default_value = std::move(create);
        return *this;
    }
    
    /** If the key for this member is not in the object when deserializing, use this \a value. If a \c default_value is
     *  not specified, the key is required.
    **/
    member_adapter_builder& default_value(TMember value)
    {
        return default_value([value] (const extraction_context&, const jsonv::value&) { return value; });
    }
    
    /** Should a \c kind::null for a key be interpreted as a missing value? **/
    member_adapter_builder& default_on_null(bool on = true)
    {
        _adapter->_default_on_null = on;
        return *this;
    }
    
    /** Only encode this member if the \a check passes. The final decision to encode is based on \e all \c check
     *  functions.
    **/
    member_adapter_builder& encode_if(std::function<bool (const serialization_context&, const T&)> check)
    {
        _adapter->add_encode_check(std::move(check));
        return *this;
    }
    
    /** Only encode this member if the \c serialization_context::version is greater than or equal to \a ver. **/
    member_adapter_builder& since(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const T&)
                         {
                             return context.version().empty() || context.version() >= ver;
                         }
                        );
    }
    
    /** Only encode this member if the \c serialization_context::version is less than or equal to \a ver. **/
    member_adapter_builder& until(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const T&)
                         {
                             return context.version().empty() || context.version() <= ver;
                         }
                        );
    }
    
    /** Only encode this member if the \c serialization_context::version is greater than \a ver. **/
    member_adapter_builder& after(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const T&)
                         {
                             return context.version().empty() || context.version() > ver;
                         }
                        );
    }
    
    /** Only encode this member if the \c serialization_context::version is less than \a ver. **/
    member_adapter_builder& before(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const T&)
                         {
                             return context.version().empty() || context.version() < ver;
                         }
                        );
    }
    
private:
    member_adapter_impl<T, TMember>* _adapter;
};

template <typename T>
class adapter_builder :
        public formats_builder_dsl
{
public:
    explicit adapter_builder(formats_builder* owner) :
            formats_builder_dsl(owner),
            _adapter(nullptr)
    {
        auto adapter = std::make_shared<adapter_impl>();
        register_adapter(adapter);
        _adapter = adapter.get();
    }
    
    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string name, TMember T::*selector)
    {
        std::unique_ptr<member_adapter_impl<T, TMember>> ptr
            (
                new member_adapter_impl<T, TMember>(std::move(name), selector)
            );
        member_adapter_builder<T, TMember> builder(formats_builder_dsl::owner, this, ptr.get());
        _adapter->_members.emplace_back(std::move(ptr));
        return builder;
    }
    
private:
    class adapter_impl :
            public adapter_for<T>
    {
    public:
    
        virtual T create(const extraction_context& context, const value& from) const override
        {
            T out;
            for (const auto& member : _members)
                member->mutate(context, from, out);
            return out;
        }
        
        virtual value encode(const serialization_context& context, const T& from) const override
        {
            value out = object();
            for (const auto& member : _members)
                member->encode(context, from, out);
            return out;
        }
        
        std::deque<std::unique_ptr<member_adapter<T>>> _members;
    };
    
private:
    adapter_impl* _adapter;
};

}

class JSONV_PUBLIC formats_builder
{
public:
    formats_builder();
    
    template <typename T>
    detail::adapter_builder<T> type()
    {
        return detail::adapter_builder<T>(this);
    }
    
    formats_builder& register_adapter(std::shared_ptr<const adapter> p)
    {
        _formats.register_adapter(std::move(p));
        return *this;
    }
    
    operator formats() const
    {
        return _formats;
    }
    
    formats_builder& reference_type(std::type_index type);
    
    /** Check that, when combined with the \c formats \a other, all types referenced by this \c formats_builder will
     *  get decoded properly.
     *  
     *  \param name if non-empty and this function throws, this \a name will be provided in the exception's \c what
     *              string. This can be useful if you are running multiple \c check_references calls and you want to
     *              name the different checks.
     *  
     *  \throws std::logic_error if \c formats this \c formats_builder is generating, when combined with the provided
     *                           \a other \c formats, cannot properly serialize all the types.
    **/
    formats_builder& check_references(formats other, const std::string& name = "");
    
private:
    formats                   _formats;
    std::set<std::type_index> _referenced_types;
};

namespace detail
{

template <typename T>
adapter_builder<T> formats_builder_dsl::type()
{
    return owner->type<T>();
}

template <typename T>
template <typename TMember>
member_adapter_builder<T, TMember> adapter_builder_dsl<T>::member(std::string name, TMember T::*selector)
{
    return owner->member(std::move(name), selector);
}

}

}

#endif/*__JSONV_SERIALIZATION_BUILDER_HPP_INCLUDED__*/

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
 *  At the end of the day, the goal is to take a C++ structure like this:
 *  
 *  \code
 *  struct person
 *  {
 *      std::string first_name;
 *      std::string last_name;
 *      int         age;
 *  };
 *  \endcode
 *  
 *  ...and easily convert it to an from a JSON representation that looks like this:
 *  
 *  \code
 *  {
 *      "first_name": "Bob",
 *      "last_name":  "Builder",
 *      "age":        29
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
 *              .member("last_name",  &person::last_name)
 *              .member("age",        &person::age)
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
 *  \paragraph serialization_builder_dsl_ref_formats_level_register_adapter register_adapter
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
**/
#ifndef __JSONV_SERIALIZATION_BUILDER_HPP_INCLUDED__
#define __JSONV_SERIALIZATION_BUILDER_HPP_INCLUDED__

#include <jsonv/config.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization_util.hpp>

#include <deque>
#include <memory>
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
            _name(std::move(name)),
            _selector(selector)
    { }
    
    virtual void mutate(const extraction_context& context, const value& from, T& out) const override
    {
        if (_default_value && from.count(_name))
            (out.*_selector) = _default_value(context, from);
        else
            (out.*_selector) = context.extract_sub<TMember>(from, _name);
    }
    
    virtual void encode(const serialization_context& context, const T& from, value& out) const override
    {
        if (should_encode(context, from))
            out.insert({ _name, context.encode(from.*_selector) });
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
    std::string                                                      _name;
    TMember T::*                                                     _selector;
    std::function<bool (const serialization_context&, const T&)>     _should_encode;
    std::function<TMember (const extraction_context&, const value&)> _default_value;
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
    { }
    
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
    
private:
    formats _formats;
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

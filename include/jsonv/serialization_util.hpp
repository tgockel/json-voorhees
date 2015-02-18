/** \file jsonv/serialization_util.hpp
 *  Helper types and functions for serialization. These are usually not needed unless you are writing your own
 *  \c extractor or \c serializer.
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_SERIALIZATION_UTIL_HPP_INCLUDED__
#define __JSONV_SERIALIZATION_UTIL_HPP_INCLUDED__

#include <jsonv/config.hpp>
#include <jsonv/serialization.hpp>

#include <type_traits>

namespace jsonv
{

/** \addtogroup Serialization
 *  \{
**/

template <typename T>
class extractor_construction :
        public extractor
{
public:
    virtual const std::type_info& get_type() const override
    {
        return typeid(T);
    }
    
    virtual void extract(const extraction_context& context,
                         const value&              from,
                         void*                     into
                        ) const override
    {
        return extract_impl<T>(context, from, into);
    }
    
protected:
    template <typename U>
    auto extract_impl(const extraction_context& context,
                      const value&              from,
                      void*                     into
                     ) const
            -> decltype(U(from, context), void())
    {
        new(into) U(from, context);
    }
    
    template <typename U, typename = void>
    auto extract_impl(const extraction_context&,
                      const value&              from,
                      void*                     into
                     ) const
            -> decltype(U(from), void())
    {
        new(into) U(from);
    }
};

template <typename T>
class extractor_for :
        public extractor
{
public:
    virtual const std::type_info& get_type() const override
    {
        return typeid(T);
    }
    
    virtual void extract(const extraction_context& context,
                         const value&              from,
                         void*                     into
                        ) const override
    {
        new(into) T(create(context, from));
    }
    
protected:
    virtual T create(const extraction_context& context, const value& from) const = 0;
};

template <typename T, typename FExtract>
class function_extractor :
        public extractor_for<T>
{
public:
    template <typename FUExtract>
    explicit function_extractor(FUExtract&& func) :
            _func(std::forward<FUExtract>(func))
    { }
    
protected:
    virtual T create(const extraction_context& context, const value& from) const override
    {
        return create_impl(_func, context, from);
    }
    
private:
    template <typename FUExtract>
    static auto create_impl(const FUExtract& func, const extraction_context& context, const value& from)
            -> decltype(func(context, from))
    {
        return func(context, from);
    }
    
    template <typename FUExtract, typename = void>
    static auto create_impl(const FUExtract& func, const extraction_context&, const value& from)
            -> decltype(func(from))
    {
        return func(from);
    }
    
private:
    FExtract _func;
};

template <typename FExtract>
auto make_extractor(FExtract func)
    -> function_extractor<decltype(func(std::declval<const extraction_context&>(), std::declval<const value&>())),
                          FExtract
                         >
{
    return function_extractor<decltype(func(std::declval<const extraction_context&>(), std::declval<const value&>())),
                              FExtract
                             >
            (std::move(func));
}

template <typename FExtract, typename = void>
auto make_extractor(FExtract func)
    -> function_extractor<decltype(func(std::declval<const value&>())),
                          FExtract
                         >
{
    return function_extractor<decltype(func(std::declval<const value&>())), FExtract>
            (std::move(func));
}

template <typename T>
class serializer_for :
        public serializer
{
public:
    virtual const std::type_info& get_type() const override
    {
        return typeid(T);
    }
    
    virtual value to_json(const serialization_context& context,
                          const void*                  from
                         ) const override
    {
        return to_json(context, *static_cast<const T*>(from));
    }
    
protected:
    virtual value to_json(const serialization_context& context,
                          const T&                     from
                         ) const = 0;
};

template <typename T, typename FToJson>
class function_serializer :
        public serializer_for<T>
{
public:
    template <typename FUToJson>
    explicit function_serializer(FUToJson&& to_json_) :
            _to_json(std::forward<FUToJson>(to_json_))
    { }
    
protected:
    
    virtual value to_json(const serialization_context& context, const T& from) const override
    {
        return to_json_impl(_to_json, context, from);
    }
    
private:
    template <typename FUToJson>
    static auto to_json_impl(const FUToJson& func, const serialization_context& context, const T& from)
            -> decltype(func(context, from))
    {
        return func(context, from);
    }
    
    template <typename FUToJson, typename = void>
    static auto to_json_impl(const FUToJson& func, const serialization_context&, const T& from)
            -> decltype(func(from))
    {
        return func(from);
    }
    
private:
    FToJson _to_json;
};

template <typename T, typename FToJson>
function_serializer<T, FToJson> make_serializer(FToJson to_json_)
{
    return function_serializer<T, FToJson>(std::move(to_json_));
}

template <typename T>
class adapter_for :
        public adapter
{
public:
    virtual const std::type_info& get_type() const override
    {
        return typeid(T);
    }
    
    virtual void extract(const extraction_context& context,
                         const value&              from,
                         void*                     into
                        ) const override
    {
        new(into) T(create(context, from));
    }
    
    virtual value to_json(const serialization_context& context,
                          const void*                  from
                         ) const override
    {
        return to_json(context, *static_cast<const T*>(from));
    }
    
protected:
    virtual T create(const extraction_context& context, const value& from) const = 0;
    
    virtual value to_json(const serialization_context& context, const T& from) const = 0;
};

template <typename T, typename FExtract, typename FToJson>
class function_adapter :
        public adapter_for<T>
{
public:
    template <typename FUExtract, typename FUToJson>
    explicit function_adapter(FUExtract&& extract_, FUToJson&& to_json_) :
            _extract(std::forward<FUExtract>(extract_)),
            _to_json(std::forward<FUToJson>(to_json_))
    { }
    
protected:
    virtual T create(const extraction_context& context, const value& from) const override
    {
        return create_impl(_extract, context, from);
    }
    
    virtual value to_json(const serialization_context& context, const T& from) const override
    {
        return to_json_impl(_to_json, context, from);
    }
    
private:
    template <typename FUExtract>
    static auto create_impl(const FUExtract& func, const extraction_context& context, const value& from)
            -> decltype(func(context, from))
    {
        return func(context, from);
    }
    
    template <typename FUExtract, typename = void>
    static auto create_impl(const FUExtract& func, const extraction_context&, const value& from)
            -> decltype(func(from))
    {
        return func(from);
    }
    
    template <typename FUToJson>
    static auto to_json_impl(const FUToJson& func, const serialization_context& context, const T& from)
            -> decltype(func(context, from))
    {
        return func(context, from);
    }
    
    template <typename FUToJson, typename = void>
    static auto to_json_impl(const FUToJson& func, const serialization_context&, const T& from)
            -> decltype(func(from))
    {
        return func(from);
    }
    
private:
    FExtract _extract;
    FToJson  _to_json;
};

template <typename FExtract, typename FToJson>
auto make_adapter(FExtract extract, FToJson to_json_)
    -> function_adapter<decltype(extract(std::declval<const extraction_context&>(), std::declval<const value&>())),
                        FExtract,
                        FToJson
                       >
{
    return function_adapter<decltype(extract(std::declval<const extraction_context&>(), std::declval<const value&>())),
                            FExtract,
                            FToJson
                           >
            (std::move(extract), std::move(to_json_));
}

template <typename FExtract, typename FToJson, typename = void>
auto make_adapter(FExtract extract, FToJson to_json_)
    -> function_adapter<decltype(extract(std::declval<const value&>())),
                        FExtract,
                        FToJson
                       >
{
    return function_adapter<decltype(extract(std::declval<const value&>())),
                            FExtract,
                            FToJson
                           >
            (std::move(extract), std::move(to_json_));
}

/** An adapter for container types. This is for convenience of creating an \c adapter for things like \c std::vector,
 *  \c std::set and such.
 *  
 *  \tparam TContainer is the container to create and encode. It must have a member type \c value_type, support
 *                     iteration and an \c insert operation.
**/
template <typename TContainer>
class container_adapter :
        public adapter_for<TContainer>
{
    using element_type = typename TContainer::value_type;
    
protected:
    virtual TContainer create(const extraction_context& context, const value& from) const override
    {
        using std::end;
        
        TContainer out;
        from.as_array(); // get nice error if input is not an array
        for (value::size_type idx = 0U; idx < from.size(); ++idx)
            out.insert(end(out), context.extract_sub<element_type>(from, idx));
        return out;
    }
    
    virtual value to_json(const serialization_context& context, const TContainer& from) const override
    {
        value out = array();
        for (const element_type& x : from)
            out.push_back(context.to_json(x));
        return out;
    }
};

/** \} **/

}

#endif/*__JSONV_SERIALIZATION_UTIL_HPP_INCLUDED__*/

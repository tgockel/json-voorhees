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
    virtual const std::type_info& get_type() const
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
    virtual const std::type_info& get_type() const
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
    
    virtual value encode(const serialization_context& context,
                         const void*                  from
                        ) const override
    {
        return encode(context, *static_cast<const T*>(from));
    }
    
protected:
    virtual value encode(const serialization_context& context,
                         const T&                     from
                        ) const = 0;
};

template <typename T, typename FEncode>
class function_serializer :
        public serializer_for<T>
{
public:
    template <typename FUEncode>
    explicit function_serializer(FUEncode&& encode_) :
            _encode(std::forward<FUEncode>(encode_))
    { }
    
protected:
    
    virtual value encode(const serialization_context& context, const T& from) const override
    {
        return encode_impl(_encode, context, from);
    }
    
private:
    template <typename FUEncode>
    static auto encode_impl(const FUEncode& func, const serialization_context& context, const T& from)
            -> decltype(func(context, from))
    {
        return func(context, from);
    }
    
    template <typename FUEncode, typename = void>
    static auto encode_impl(const FUEncode& func, const serialization_context&, const T& from)
            -> decltype(func(from))
    {
        return func(from);
    }
    
private:
    FEncode _encode;
};

template <typename T, typename FEncode>
function_serializer<T, FEncode> make_serializer(FEncode encode)
{
    return function_serializer<T, FEncode>(std::move(encode));
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
    
    virtual value encode(const serialization_context& context,
                         const void*                  from
                        ) const override
    {
        return encode(context, *static_cast<const T*>(from));
    }
    
protected:
    virtual T create(const extraction_context& context, const value& from) const = 0;
    
    virtual value encode(const serialization_context& context, const T& from) const = 0;
};

template <typename T, typename FExtract, typename FEncode>
class function_adapter :
        public adapter_for<T>
{
public:
    template <typename FUExtract, typename FUEncode>
    explicit function_adapter(FUExtract&& extract_, FUEncode&& encode_) :
            _extract(std::forward<FUExtract>(extract_)),
            _encode(std::forward<FUEncode>(encode_))
    { }
    
protected:
    virtual T create(const extraction_context& context, const value& from) const override
    {
        return create_impl(_extract, context, from);
    }
    
    virtual value encode(const serialization_context& context, const T& from) const override
    {
        return encode_impl(_encode, context, from);
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
    
    template <typename FUEncode>
    static auto encode_impl(const FUEncode& func, const serialization_context& context, const T& from)
            -> decltype(func(context, from))
    {
        return func(context, from);
    }
    
    template <typename FUEncode, typename = void>
    static auto encode_impl(const FUEncode& func, const serialization_context&, const T& from)
            -> decltype(func(from))
    {
        return func(from);
    }
    
private:
    FExtract _extract;
    FEncode  _encode;
};

template <typename FExtract, typename FEncode>
auto make_adapter(FExtract extract, FEncode encode)
    -> function_adapter<decltype(extract(std::declval<const extraction_context&>(), std::declval<const value&>())),
                        FExtract,
                        FEncode
                       >
{
    return function_adapter<decltype(extract(std::declval<const extraction_context&>(), std::declval<const value&>())),
                            FExtract,
                            FEncode
                           >
            (std::move(extract), std::move(encode));
}

template <typename FExtract, typename FEncode, typename = void>
auto make_adapter(FExtract extract, FEncode encode)
    -> function_adapter<decltype(extract(std::declval<const value&>())),
                        FExtract,
                        FEncode
                       >
{
    return function_adapter<decltype(extract(std::declval<const value&>())),
                            FExtract,
                            FEncode
                           >
            (std::move(extract), std::move(encode));
}

/** \} **/

}

#endif/*__JSONV_SERIALIZATION_UTIL_HPP_INCLUDED__*/

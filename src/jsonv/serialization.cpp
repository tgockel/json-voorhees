/** \file
 *  Conversion between C++ types and JSON values.
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/serialization.hpp>
#include <jsonv/coerce.hpp>
#include <jsonv/value.hpp>

#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extractor                                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extractor::~extractor() noexcept = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extraction_error                                                                                                   //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string make_extraction_error_errmsg(const extraction_context& context, const std::string& message)
{
    std::ostringstream os;
    os << "Extraction error";
    
    if (!context.path().empty())
        os << " at " << context.path();
    
    if (!message.empty())
        os << ": " << message;
    
    return os.str();
}

extraction_error::extraction_error(const extraction_context& context, const std::string& message) :
        std::runtime_error(make_extraction_error_errmsg(context, message)),
        std::nested_exception(),
        _path(context.path())
{ }

extraction_error::~extraction_error() noexcept = default;

const path& extraction_error::path() const
{
    return _path;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// no_extractor                                                                                                       //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string make_no_extractor_errmsg(const std::type_info& type)
{
    std::ostringstream ss;
    ss << "Could not find extractor for type: " << type.name();
    return ss.str();
}

no_extractor::no_extractor(const std::type_info& type) :
        runtime_error(make_no_extractor_errmsg(type)),
        _type_index(type)
{ }

no_extractor::~no_extractor() noexcept
{ }

std::type_index no_extractor::type_index() const
{
    return _type_index;
}

const char* no_extractor::type_name() const
{
    return _type_index.name();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats::data                                                                                                      //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct JSONV_LOCAL formats::data
{
public:
    using roots_list = std::vector<std::shared_ptr<const data>>;
    
    using extractor_map        = std::unordered_map<std::type_index, const extractor*>;
    using shared_extractor_set = std::unordered_set<std::shared_ptr<const extractor>>;
    
public:
    /// The previous data this comes from...this allows us to make a huge tree of formats with custom extension points.
    roots_list roots;
    
    extractor_map extractors;
    
    shared_extractor_set shared_extractors;
    
    explicit data(roots_list roots) :
            roots(std::move(roots))
    { }
    
public:
    const extractor* find_extractor(const std::type_index& typeidx) const
    {
        auto iter = extractors.find(typeidx);
        if (iter != end(extractors))
        {
            return iter->second;
        }
        else
        {
            for (const auto& sub : roots)
            {
                auto ptr = sub->find_extractor(typeidx);
                if (ptr)
                    return ptr;
            }
            return nullptr;
        }
    }
    
public:
    extractor_map::iterator insert_extractor(const extractor* ex)
    {
        std::type_index typeidx(ex->get_type());
        auto iter = extractors.find(typeidx);
        if (iter != end(extractors))
        {
            // if we already have a value, search the shared maps to see if it is present
            std::ostringstream os;
            os << "Already have an extractor for type " << typeidx.name();
            throw std::invalid_argument(os.str());
        }
        else
        {
            return extractors.emplace(typeidx, ex).first;
        }
    }
    
    void insert_extractor(std::shared_ptr<const extractor> ex)
    {
        auto iter = insert_extractor(ex.get());
        auto rollback = detail::on_scope_exit([this, &iter] { extractors.erase(iter); });
        shared_extractors.insert(std::move(ex));
        rollback.release();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats                                                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

formats::formats(data::roots_list roots) :
        _data(std::make_shared<data>(std::move(roots)))
{ }

formats::formats() :
        formats(data::roots_list())
{ }

formats formats::compose(const list& bases)
{
    data::roots_list roots;
    roots.reserve(bases.size());
    std::transform(begin(bases), end(bases),
                   std::back_inserter(roots),
                   [] (const formats& fmt) { return fmt._data; }
                  );
    return formats(std::move(roots));
}

formats::~formats() noexcept
{ }

void formats::extract(const std::type_info&     type,
                      const value&              from,
                      void*                     into,
                      const extraction_context& context
                     ) const
{
    const extractor* ex = _data->find_extractor(std::type_index(type));
    if (ex)
        ex->extract(context, from, into);
    else
        throw no_extractor(type);
}

void formats::register_extractor(const extractor* ex)
{
    _data->insert_extractor(ex);
}

void formats::register_extractor(std::shared_ptr<const extractor> ex)
{
    _data->insert_extractor(std::move(ex));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats::defaults                                                                                                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
static void register_integer_extractor(formats& fmt)
{
    static auto instance = make_function_extractor([] (const value& from) { return T(from.as_integer()); });
    fmt.register_extractor(&instance);
}

static formats create_default_formats()
{
    formats fmt;
    
    static auto json_extractor = make_function_extractor([] (const value& from) { return from; });
    fmt.register_extractor(&json_extractor);
    
    static auto string_extractor = make_function_extractor([] (const value& from) { return from.as_string(); });
    fmt.register_extractor(&string_extractor);
    
    static auto bool_extractor = make_function_extractor([] (const value& from) { return from.as_boolean(); });
    fmt.register_extractor(&bool_extractor);
    
    register_integer_extractor<std::int8_t>(fmt);
    register_integer_extractor<std::uint8_t>(fmt);
    register_integer_extractor<std::int16_t>(fmt);
    register_integer_extractor<std::uint16_t>(fmt);
    register_integer_extractor<std::int32_t>(fmt);
    register_integer_extractor<std::uint32_t>(fmt);
    register_integer_extractor<std::int64_t>(fmt);
    register_integer_extractor<std::uint64_t>(fmt);
    
    static auto double_extractor = make_function_extractor([] (const value& from) { return from.as_decimal(); });
    fmt.register_extractor(&double_extractor);
    static auto float_extractor = make_function_extractor([] (const value& from) { return float(from.as_decimal()); });
    fmt.register_extractor(&float_extractor);
    
    return fmt;
}

static const formats& default_formats_ref()
{
    static formats instance = create_default_formats();
    return instance;
}

formats formats::defaults()
{
    return formats({ default_formats_ref() });
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats::global                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static formats& global_formats_ref()
{
    static formats instance = default_formats_ref();
    return instance;
}

formats formats::global()
{
    return formats({ global_formats_ref() });
}

void formats::set_global(formats fmt)
{
    global_formats_ref() = std::move(fmt);
}

void formats::reset_global()
{
    global_formats_ref() = default_formats_ref();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats::coerce                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
static void register_integer_coerce_extractor(formats& fmt)
{
    static auto instance = make_function_extractor([] (const value& from) { return T(coerce_integer(from)); });
    fmt.register_extractor(&instance);
}

static formats create_coerce_formats()
{
    formats fmt;
    
    static auto json_extractor = make_function_extractor([] (const value& from) { return from; });
    fmt.register_extractor(&json_extractor);
    
    static auto string_extractor = make_function_extractor([] (const value& from) { return coerce_string(from); });
    fmt.register_extractor(&string_extractor);
    
    static auto bool_extractor = make_function_extractor([] (const value& from) { return coerce_boolean(from); });
    fmt.register_extractor(&bool_extractor);
    
    register_integer_coerce_extractor<std::int8_t>(fmt);
    register_integer_coerce_extractor<std::uint8_t>(fmt);
    register_integer_coerce_extractor<std::int16_t>(fmt);
    register_integer_coerce_extractor<std::uint16_t>(fmt);
    register_integer_coerce_extractor<std::int32_t>(fmt);
    register_integer_coerce_extractor<std::uint32_t>(fmt);
    register_integer_coerce_extractor<std::int64_t>(fmt);
    register_integer_coerce_extractor<std::uint64_t>(fmt);
    
    static auto double_extractor = make_function_extractor([] (const value& from) { return coerce_decimal(from); });
    fmt.register_extractor(&double_extractor);
    static auto float_extractor = make_function_extractor([] (const value& from) { return float(coerce_decimal(from)); });
    fmt.register_extractor(&float_extractor);
    
    return fmt;
}

static const formats& coerce_formats_ref()
{
    static formats instance = create_coerce_formats();
    return instance;
}

formats formats::coerce()
{
    return formats({ coerce_formats_ref() });
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extraction_context                                                                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extraction_context::extraction_context(const jsonv::formats& fmt, const jsonv::version& ver, jsonv::path p) :
        _formats(fmt),
        _version(ver),
        _path(std::move(p))
{ }

extraction_context::extraction_context() :
        extraction_context(global_formats_ref())
{ }

}

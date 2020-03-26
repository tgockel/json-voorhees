/// \file
/// Conversion between C++ types and JSON values.
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/serialization.hpp>
#include <jsonv/coerce.hpp>
#include <jsonv/demangle.hpp>
#include <jsonv/detail/clamp.hpp>
#include <jsonv/serialization/function_adapter.hpp>
#include <jsonv/serialization/function_extractor.hpp>
#include <jsonv/serialization/function_serializer.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/value.hpp>

#include <cstdint>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// serializer                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

serializer::~serializer() noexcept = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// adapter                                                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

adapter::~adapter() noexcept = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// no_extractor                                                                                                       //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string make_no_serializer_extractor_errmsg(const char* kind, const std::type_index& type)
{
    std::ostringstream ss;
    ss << "Could not find " << kind << " for type: " << demangle(type.name());
    return ss.str();
}

no_extractor::no_extractor(const std::type_index& type) :
        runtime_error(make_no_serializer_extractor_errmsg("extractor", type)),
        _type_index(type),
        _type_name(demangle(type.name()))
{ }

no_extractor::no_extractor(const std::type_info& type) :
        no_extractor(std::type_index(type))
{ }

no_extractor::~no_extractor() noexcept
{ }

std::type_index no_extractor::type_index() const
{
    return _type_index;
}

string_view no_extractor::type_name() const
{
    return _type_name;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// no_serializer                                                                                                      //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

no_serializer::no_serializer(const std::type_index& type) :
        runtime_error(make_no_serializer_extractor_errmsg("serializer", type)),
        _type_index(type),
        _type_name(demangle(type.name()))
{ }

no_serializer::no_serializer(const std::type_info& type) :
        no_serializer(std::type_index(type))
{ }

no_serializer::~no_serializer() noexcept
{ }

std::type_index no_serializer::type_index() const
{
    return _type_index;
}

string_view no_serializer::type_name() const
{
    return _type_name;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extract_options                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extract_options::extract_options() noexcept = default;

extract_options::~extract_options() noexcept = default;

extract_options extract_options::create_default()
{
    return extract_options();
}

extract_options& extract_options::failure_mode(on_error mode)
{
    _failure_mode = mode;
    return *this;
}

extract_options& extract_options::max_failures(size_type limit)
{
    _max_failures = limit;
    return *this;
}

extract_options& extract_options::on_duplicate_key(duplicate_key_action action)
{
    _on_duplicate_key = action;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats::data                                                                                                      //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct JSONV_LOCAL formats::data
{
public:
    using roots_list      = std::vector<std::shared_ptr<const data>>;
    using extractor_map   = std::unordered_map<std::type_index, const extractor*>;
    using serializer_map  = std::unordered_map<std::type_index, const serializer*>;
    using owned_items_set = std::unordered_set<std::shared_ptr<const void>>;

public:
    /// The previous data this comes from...this allows us to make a huge tree of formats with custom extension points.
    roots_list roots;

    extractor_map extractors;

    owned_items_set owned_items;

    serializer_map serializers;

    explicit data(roots_list roots) :
            roots(std::move(roots))
    { }

public:
    const extractor* find_extractor(const std::type_index& typeidx) const
    {
        return find_impl<extractor>(this,
                                    typeidx,
                                    [] (const data* self) -> const extractor_map& { return self->extractors; }
                                   );
    }

    const serializer* find_serializer(const std::type_index& typeidx) const
    {
        return find_impl<serializer>(this,
                                     typeidx,
                                     [] (const data* self) -> const serializer_map& { return self->serializers; }
                                    );
    }

    template <typename T, typename FSelectMap>
    static const T* find_impl(const data* self,
                              const std::type_index& typeidx,
                              const FSelectMap&      select_map
                             )
    {
        const auto& map = select_map(self);
        auto iter = map.find(typeidx);
        if (iter != std::end(map))
        {
            return iter->second;
        }
        else
        {
            for (const auto& sub : self->roots)
            {
                auto ptr = find_impl<T>(sub.get(), typeidx, select_map);
                if (ptr)
                    return ptr;
            }
            return nullptr;
        }
    }

public:
    extractor_map::iterator insert_extractor(const extractor* ex, duplicate_type_action action)
    {
        std::type_index typeidx(ex->get_type());
        auto iter = extractors.find(typeidx);
        if (iter != end(extractors))
        {
            if (duplicate_type_action::exception == action)
            {
                throw duplicate_type_error("an extractor", typeidx);
            }
            else if (duplicate_type_action::replace == action)
            {
                iter->second = ex;
            }

            return iter;
        }
        else
        {
            return extractors.emplace(typeidx, ex).first;
        }
    }

    void insert_extractor(std::shared_ptr<const extractor> ex, duplicate_type_action action)
    {
        auto iter = insert_extractor(ex.get(), action);
        auto rollback = detail::on_scope_exit([this, &iter] { extractors.erase(iter); });
        owned_items.insert(std::move(ex));
        rollback.release();
    }

    serializer_map::iterator insert_serializer(const serializer* ser, duplicate_type_action action)
    {
        std::type_index typeidx(ser->get_type());
        auto iter = serializers.find(typeidx);
        if (iter != end(serializers))
        {
            if (duplicate_type_action::exception == action)
            {
                throw duplicate_type_error("a serializer", typeidx);
            }
            else if (duplicate_type_action::replace == action)
            {
                iter->second = ser;
            }

            return iter;
        }
        else
        {
            return serializers.emplace(typeidx, ser).first;
        }
    }

    void insert_serializer(std::shared_ptr<const serializer> ser, duplicate_type_action action)
    {
        auto iter = insert_serializer(ser.get(), action);
        auto rollback = detail::on_scope_exit([this, &iter] { serializers.erase(iter); });
        owned_items.insert(std::move(ser));
        rollback.release();
    }

    void insert_adapter(const adapter* adp, duplicate_type_action action)
    {
        auto iter = insert_extractor(adp, action);
        auto rollback = detail::on_scope_exit([this, &iter] { extractors.erase(iter); });
        insert_serializer(adp, action);
        rollback.release();
    }

    void insert_adapter(std::shared_ptr<const adapter> adp, duplicate_type_action action)
    {
        auto iter_ex = insert_extractor(adp.get(), action);
        auto rollback_ex = detail::on_scope_exit([this, &iter_ex] { extractors.erase(iter_ex); });
        auto iter_ser = insert_serializer(adp.get(), action);
        auto rollback_ser = detail::on_scope_exit([this, &iter_ser] { serializers.erase(iter_ser); });
        owned_items.insert(std::move(adp));
        rollback_ex.release();
        rollback_ser.release();
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

const extractor& formats::get_extractor(std::type_index type) const
{
    const extractor* ex = _data->find_extractor(type);
    if (ex)
        return *ex;
    else
        throw no_extractor(type);
}

const extractor& formats::get_extractor(const std::type_info& type) const
{
    return get_extractor(std::type_index(type));
}

const serializer& formats::get_serializer(std::type_index type) const
{
    const serializer* ser = _data->find_serializer(type);
    if (ser)
        return *ser;
    else
        throw no_serializer(type);
}

const serializer& formats::get_serializer(const std::type_info& type) const
{
    return get_serializer(std::type_index(type));
}

value formats::to_json(const std::type_info& type,
                       const void* from,
                       const serialization_context& context
                      ) const
{
    return get_serializer(type).to_json(context, from);
}

void formats::register_extractor(const extractor* ex, duplicate_type_action action)
{
    _data->insert_extractor(ex, action);
}

void formats::register_extractor(std::shared_ptr<const extractor> ex, duplicate_type_action action)
{
    _data->insert_extractor(std::move(ex), action);
}

void formats::register_serializer(const serializer* ser, duplicate_type_action action)
{
    _data->insert_serializer(ser, action);
}

void formats::register_serializer(std::shared_ptr<const serializer> ser, duplicate_type_action action)
{
    _data->insert_serializer(std::move(ser), action);
}

void formats::register_adapter(const adapter* adp, duplicate_type_action action)
{
    _data->insert_adapter(adp, action);
}

void formats::register_adapter(std::shared_ptr<const adapter> adp, duplicate_type_action action)
{
    _data->insert_adapter(std::move(adp), action);
}

bool formats::operator==(const formats& other) const
{
    return _data.get() == other._data.get();
}

bool formats::operator!=(const formats& other) const
{
    return !operator==(other);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats::defaults                                                                                                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
static void register_integer_adapter(formats&              fmt,
                                     duplicate_type_action on_duplicate = duplicate_type_action::exception
                                    )
{
    static auto instance =
        make_adapter([] (extraction_context& cxt, reader& from) -> result<T, void>
                     {
                         return cxt.current_as<ast_node::integer>(from)
                            .map([](const ast_node::integer& x)
                            {
                                // TODO: Bounds checking or something
                                return T(x.value());
                            });
                     },
                     [] (const T& from) { return value(static_cast<std::int64_t>(from)); }
                    );
    fmt.register_adapter(&instance, on_duplicate);
}

static formats create_default_formats()
{
    formats fmt;

    // TODO: extract a whole tree
    //static auto json_extractor = make_adapter([] (const value& from) { return from; },
    //                                          [] (const value& from) { return from; }
    //                                         );
    //fmt.register_adapter(&json_extractor);

    static auto string_extractor =
        make_adapter([] (extraction_context& cxt, reader& from) -> result<std::string, void>
                     {
                        if (!cxt.expect(from, { ast_node_type::string_canonical, ast_node_type::string_escaped }))
                            return error{};


                        if (from.current().type() == ast_node_type::string_canonical)
                        {
                            return ok(std::string(from.current_as<ast_node::string_canonical>()->value()));
                        }
                        else
                        {
                            return ok(from.current_as<ast_node::string_escaped>()->value());
                        }
                     },
                     [] (const std::string& from) { return value(from); }
                    );
    fmt.register_adapter(&string_extractor);

    static auto string_view_adapter =
        make_adapter([] (extraction_context& cxt, reader& from)
                     {
                         return cxt.current_as<ast_node::string_canonical>(from)
                            .map([](const auto& x) { return x.value(); });
                     },
                     [] (const string_view& from) { return value(from); }
                    );
    fmt.register_adapter(&string_view_adapter);

    static auto cchar_ptr_serializer = make_serializer<const char*>([] (const char* from) { return value(from); });
    fmt.register_serializer(&cchar_ptr_serializer);
    static auto char_ptr_serializer = make_serializer<char*>([] (char* from) { return value(from); });
    fmt.register_serializer(&char_ptr_serializer);

    static auto bool_extractor =
        make_adapter([] (extraction_context& cxt, reader& from) -> result<bool, void>
                     {
                         if (!cxt.expect(from, { ast_node_type::literal_false, ast_node_type::literal_true }))
                            return error{};

                         return from.current().type() == ast_node_type::literal_true;
                     },
                     [] (const bool& from) { return value(from); }
                    );
    fmt.register_adapter(&bool_extractor);

    register_integer_adapter<std::int8_t>(fmt);
    register_integer_adapter<std::uint8_t>(fmt);
    register_integer_adapter<std::int16_t>(fmt);
    register_integer_adapter<std::uint16_t>(fmt);
    register_integer_adapter<std::int32_t>(fmt);
    register_integer_adapter<std::uint32_t>(fmt);
    register_integer_adapter<std::int64_t>(fmt);
    register_integer_adapter<std::uint64_t>(fmt);

    // These common types are usually covered by the explicitly-sized integers, but try to add them for platforms like
    // OSX and Windows.
    register_integer_adapter<std::size_t>(fmt, duplicate_type_action::ignore);
    register_integer_adapter<std::ptrdiff_t>(fmt, duplicate_type_action::ignore);
    register_integer_adapter<long>(fmt, duplicate_type_action::ignore);
    register_integer_adapter<unsigned long>(fmt, duplicate_type_action::ignore);

    static auto double_extractor =
        make_adapter([] (extraction_context& cxt, reader& from) -> result<double, void>
                     {
                         return cxt.current_as<ast_node::decimal>(from)
                            .map([] (const auto& x) { return x.value(); });
                     },
                     [] (const double& from) { return value(from); }
                    );
    fmt.register_adapter(&double_extractor);
    static auto float_extractor =
        make_adapter([] (extraction_context& cxt, reader& from) -> result<float, void>
                     {
                         return cxt.current_as<ast_node::decimal>(from)
                            .map([] (const auto& x) { return float(x.value()); });
                     },
                     [] (const float& from) { return value(from); }
                    );
    fmt.register_adapter(&float_extractor);

    return fmt;
}

static const formats& default_formats_ref()
{
    static formats instance = create_default_formats();
    return instance;
}

formats formats::defaults()
{
    return formats::compose({ default_formats_ref() });
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
    return formats::compose({ global_formats_ref() });
}

formats formats::set_global(formats fmt)
{
    using std::swap;

    swap(global_formats_ref(), fmt);
    return fmt;
}

formats formats::reset_global()
{
    return set_global(default_formats_ref());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats::coerce                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
static void register_integer_coerce_extractor(formats&              fmt,
                                              duplicate_type_action on_duplicate = duplicate_type_action::exception)
{
    static auto instance =
        make_extractor([] (extraction_context& cxt, reader& from) -> result<T, void>
                       {
                           const auto& current = from.current();
                           switch (current.type())
                           {
                           case ast_node_type::literal_true:
                               return ok(T(1));
                           case ast_node_type::literal_false:
                           case ast_node_type::literal_null:
                               return ok(T(0));
                           case ast_node_type::decimal:
                           {
                               auto value = current.as<ast_node::decimal>().value();
                               if (value > double(std::numeric_limits<T>::max()))
                                   return ok(std::numeric_limits<T>::max());
                               else if (value < double(std::numeric_limits<T>::min()))
                                   return ok(std::numeric_limits<T>::min());
                               else
                                   return ok(T(value));
                           }
                           case ast_node_type::integer:
                               return ok(detail::clamp_cast<T>(current.as<ast_node::integer>().value()));
                           default:
                               return cxt.problem(from.current_path(), "Could not coerce integer");
                           }
                       }
                      );
    fmt.register_extractor(&instance, on_duplicate);
}

static formats create_coerce_formats()
{
    formats fmt;

    static auto string_extractor =
        make_extractor([](reader& from) -> result<std::string, void> { return ok(coerce_string(from)); });
    fmt.register_extractor(&string_extractor);

    static auto bool_extractor =
        make_extractor([] (reader& from) -> result<bool, void> { return ok(coerce_boolean(from)); });
    fmt.register_extractor(&bool_extractor);

    register_integer_coerce_extractor<std::int8_t>(fmt);
    register_integer_coerce_extractor<std::uint8_t>(fmt);
    register_integer_coerce_extractor<std::int16_t>(fmt);
    register_integer_coerce_extractor<std::uint16_t>(fmt);
    register_integer_coerce_extractor<std::int32_t>(fmt);
    register_integer_coerce_extractor<std::uint32_t>(fmt);
    register_integer_coerce_extractor<std::int64_t>(fmt);
    register_integer_coerce_extractor<std::uint64_t>(fmt);

    // These common types are usually covered by the explicitly-sized integers, but try to add them for platforms like
    // OSX and Windows.
    register_integer_coerce_extractor<std::size_t>(fmt, duplicate_type_action::ignore);
    register_integer_coerce_extractor<std::ptrdiff_t>(fmt, duplicate_type_action::ignore);
    register_integer_coerce_extractor<long>(fmt, duplicate_type_action::ignore);
    register_integer_coerce_extractor<unsigned long>(fmt, duplicate_type_action::ignore);

    static auto double_extractor =
        make_extractor([] (reader& from) -> result<double, void> { return coerce_decimal(from); });
    fmt.register_extractor(&double_extractor);
    static auto float_extractor =
        make_extractor([] (reader& from) -> result<float, void> { return float(coerce_decimal(from)); });
    fmt.register_extractor(&float_extractor);

    return fmt;
}

static const formats& coerce_formats_ref()
{
    static formats instance = formats::compose({ create_coerce_formats(), default_formats_ref() });
    return instance;
}

formats formats::coerce()
{
    return formats::compose({ coerce_formats_ref() });
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// context_base                                                                                                       //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

context_base::context_base(jsonv::formats        fmt,
                           const jsonv::version& ver,
                           const void*           userdata
                          ) :
        _formats(std::move(fmt)),
        _version(ver),
        _user_data(userdata)
{ }

context_base::context_base() :
        context_base(formats::global())
{ }

context_base::~context_base() noexcept = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// serialization_context                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

serialization_context::serialization_context(jsonv::formats        fmt,
                                             const jsonv::version& ver,
                                             const void*           userdata
                                            ) :
        context_base(std::move(fmt), ver, userdata)
{ }

serialization_context::serialization_context() :
        context_base()
{ }

serialization_context::~serialization_context() noexcept = default;

value serialization_context::to_json(const std::type_info& type, const void* from) const
{
    return formats().to_json(type, from, *this);
}

}

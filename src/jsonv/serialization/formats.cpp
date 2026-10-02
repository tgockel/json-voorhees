/// \file
/// The \c formats registry and the exceptions it raises.
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/serialization/formats.hpp>
#include <jsonv/demangle.hpp>
#include <jsonv/detail/scope_exit.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/value.hpp>

#include <expected>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// duplicate_type_error                                                                                               //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string make_duplicate_type_errmsg(const std::string& operation, const std::type_index& type)
{
    std::ostringstream os;
    os << "Already have " << operation << " for type " << demangle(type.name());
    return os.str();
}

duplicate_type_error::duplicate_type_error(const std::string& operation, const std::type_index& type) :
        std::invalid_argument(make_duplicate_type_errmsg(operation, type)),
        _type_index(type)
{ }

duplicate_type_error::~duplicate_type_error() noexcept = default;

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

std::string_view no_extractor::type_name() const
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

std::string_view no_serializer::type_name() const
{
    return _type_name;
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
    /// What \c insert_extractor or \c insert_serializer did to the entry for one type, so that a registration which
    /// fails further on can put the entry back as it found it. The iterator stays good because a registration changes
    /// each map at most once and does nothing to that map afterward.
    template <typename TMap>
    struct insertion
    {
        TMap&                      entries;
        typename TMap::iterator    entry;
        /// What the entry held before, or null when the insertion added it. An \c ignore leaves the entry holding just
        /// this, which is why reverting one changes nothing.
        typename TMap::mapped_type previous;

        void revert() noexcept
        {
            if (previous)
                entry->second = previous;
            else
                entries.erase(entry);
        }
    };

    insertion<extractor_map> insert_extractor(const extractor* ex, duplicate_type_action action)
    {
        std::type_index typeidx(ex->get_type());
        auto iter = extractors.find(typeidx);
        if (iter != end(extractors))
        {
            if (duplicate_type_action::exception == action)
            {
                throw duplicate_type_error("an extractor", typeidx);
            }

            const extractor* previous = iter->second;
            if (duplicate_type_action::replace == action)
            {
                iter->second = ex;
            }

            return { extractors, iter, previous };
        }
        else
        {
            return { extractors, extractors.emplace(typeidx, ex).first, nullptr };
        }
    }

    void insert_extractor(std::shared_ptr<const extractor> ex, duplicate_type_action action)
    {
        auto inserted = insert_extractor(ex.get(), action);
        auto rollback = detail::on_scope_exit([&inserted] { inserted.revert(); });
        owned_items.insert(std::move(ex));
        rollback.release();
    }

    insertion<serializer_map> insert_serializer(const serializer* ser, duplicate_type_action action)
    {
        std::type_index typeidx(ser->get_type());
        auto iter = serializers.find(typeidx);
        if (iter != end(serializers))
        {
            if (duplicate_type_action::exception == action)
            {
                throw duplicate_type_error("a serializer", typeidx);
            }

            const serializer* previous = iter->second;
            if (duplicate_type_action::replace == action)
            {
                iter->second = ser;
            }

            return { serializers, iter, previous };
        }
        else
        {
            return { serializers, serializers.emplace(typeidx, ser).first, nullptr };
        }
    }

    void insert_serializer(std::shared_ptr<const serializer> ser, duplicate_type_action action)
    {
        auto inserted = insert_serializer(ser.get(), action);
        auto rollback = detail::on_scope_exit([&inserted] { inserted.revert(); });
        owned_items.insert(std::move(ser));
        rollback.release();
    }

    void insert_adapter(const adapter* adp, duplicate_type_action action)
    {
        auto inserted = insert_extractor(adp, action);
        auto rollback = detail::on_scope_exit([&inserted] { inserted.revert(); });
        insert_serializer(adp, action);
        rollback.release();
    }

    void insert_adapter(std::shared_ptr<const adapter> adp, duplicate_type_action action)
    {
        auto inserted_ex = insert_extractor(adp.get(), action);
        auto rollback_ex = detail::on_scope_exit([&inserted_ex] { inserted_ex.revert(); });
        auto inserted_ser = insert_serializer(adp.get(), action);
        auto rollback_ser = detail::on_scope_exit([&inserted_ser] { inserted_ser.revert(); });
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

std::expected<void, ast_node_type> formats::extract(const std::type_info& type,
                                                    reader&               from,
                                                    void*                 into,
                                                    extraction_context&   context
                                                   ) const
{
    return get_extractor(type).extract(context, from, into);
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

}

/// \file jsonv/serialization_builder.hpp
/// DSL for building \c formats.
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/demangle.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization/container_adapter.hpp>
#include <jsonv/serialization/enum_adapter.hpp>
#include <jsonv/serialization/optional_adapter.hpp>
#include <jsonv/serialization/polymorphic_adapter.hpp>
#include <jsonv/serialization/wrapper_adapter.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace jsonv
{

/// \page serialization_builder_dsl Serialization Builder DSL
///
/// Most applications tend to have a lot of structure types. While it is possible to write an \c extractor and
/// \c serializer (or \c adapter) for each type, this can get a little bit tedious. Beyond that, it is very difficult to
/// look at the contents of adapter code and discover what the JSON might actually look like. The builder DSL is meant
/// to solve these issues by providing a convenient way to describe conversion operations for your C++ types.
///
/// At the end of the day, the goal is to take some C++ structures like this:
///
/// \code
/// struct person
/// {
///     std::string first_name;
///     std::string last_name;
///     int         age;
///     std::string role;
/// };
///
/// struct company
/// {
///     std::string         name;
///     bool                certified;
///     std::vector<person> employees;
///     std::list<person>   candidates;
/// };
/// \endcode
///
/// ...and easily convert it to an from a JSON representation that looks like this:
///
/// \code
/// {
///     "name": "Paul's Construction",
///     "certified": false,
///     "employees": [
///         {
///             "first_name": "Bob",
///             "last_name":  "Builder",
///             "age":        29
///         },
///         {
///             "first_name": "James",
///             "last_name":  "Johnson",
///             "age":        38,
///             "role":       "Foreman"
///         }
///     ],
///     "candidates": [
///         {
///             "firstname": "Adam",
///             "lastname":  "Ant"
///         }
///     ]
/// }
/// \endcode
///
/// To define a \c formats for this \c person type using the serialization builder DSL, you would say:
///
/// \code
/// jsonv::formats fmts =
///     jsonv::formats_builder()
///         .type<person>()
///             .member("first_name", &person::first_name)
///                 .alternate_name("firstname")
///             .member("last_name",  &person::last_name)
///                 .alternate_name("lastname")
///             .member("age",        &person::age)
///                 .until({ 6,1 })
///                 .default_value(21)
///                 .default_on_null()
///                 .check_input([] (int value) { if (value < 0) throw std::logic_error("Age must be positive."); })
///             .member("role",       &person::role)
///                 .since({ 2,0 })
///                 .default_value("Builder")
///         .type<company>()
///             .member("name",       &company::name)
///             .member("certified",  &company::certified)
///             .member("employees",  &company::employees)
///             .member("candidates", &company::candidates)
///         .register_containers<company, std::vector, std::list>()
///         .check_references(jsonv::formats::defaults())
///     ;
/// \endcode
///
/// \section Reference
///
/// The DSL is made up of three major parts:
///
///  1. \e formats -- modifies a \c jsonv::formats object by adding new type adapters to it
///  2. \e type -- modifies the behavior of a \c jsonv::adapter by adding new members to it
///  3. \e member -- modifies an individual member inside of a specific type
///
/// Each successive function call transforms your context. \e Narrowing calls make your context more specific; for
/// example, calling \c type from a \e formats context allows you to modify a specific type. \e Widening calls make the
/// context less specific and are always available; for example, when in the \e member context, you can still call
/// \c type from the \e formats context to specify a new type.
///
/// \dot
/// digraph serialization_builder_dsl {
///   formats  [label="formats"]
///   type     [label="type"]
///   member   [label="member"]
///
///   formats -> formats
///   formats -> type
///   type    -> formats
///   type    -> type
///   type    -> member
///   member  -> formats
///   member  -> type
///   member  -> member
/// }
/// \enddot
///
/// \subsection serialization_builder_dsl_ref_formats Formats Context
///
/// Commands in this section modify the behavior of the underlying \c jsonv::formats object.
///
/// \subsubsection serialization_builder_dsl_ref_formats_level Level
///
/// \paragraph serialization_builder_dsl_ref_formats_level_check_references check_references
///
///  - <tt>check_references(formats)</tt>
///  - <tt>check_references(formats, std::string name)</tt>
///  - <tt>check_references(formats::list)</tt>
///  - <tt>check_references(formats::list, std::string name)</tt>
///  - <tt>check_references()</tt>
///  - <tt>check_references(std::string name)</tt>
///
/// Tests that every type referenced by the members of the output of the DSL have an \c extractor and a \c serializer.
/// The provided \c formats is used to draw extra types from (a common value is \c jsonv::formats::defaults). In other
/// words, it asks the question: If the \c formats from this DSL was combined with these other \c formats, could all of
/// the types be encoded and decoded?
///
/// This does not mutate the DSL in any way. On successful verification, it will appear that nothing happened. If the
/// verification is not successful, an exception will be thrown with the offending types in the message. For example:
///
/// \code
/// There are 2 types referenced that the formats do not know how to serialize:
///  - date_type (referenced by: name_space::foo, other::name::space::bar)
///  - tree
/// \endcode
///
/// If \a name is provided, the value will be output to the error message on failure. This can be useful if you have
/// multiple \c check_references statements and wish to more easily determine the failing \c formats combination from
/// the error message alone.
///
/// \note
/// This is evaluated \e immediately, so it is best to call this function as the very last step in the DSL.
///
/// \code
///   .check_references(jsonv::formats::defaults())
/// \endcode
///
/// \paragraph serialization_builder_dsl_ref_formats_level_reference_type reference_type
///
///  - <tt>reference_type(std::type_index type)</tt>
///  - <tt>reference_type(std::type_index type, std::type_index from)</tt>
///
/// Explicitly add a reference to the provided \a type in the DSL. If \a from is provided, also add a back reference for
/// tracking purposes. The \a from field is useful for tracking \e why the \a type is referenced.
///
/// Type references are used in \ref serialization_builder_dsl_ref_formats_level_check_references to both check and
/// generate error messages if the \c formats the DSL is building cannot fully create and extract JSON values. You do
/// not usually have to call this, as each call to \ref serialization_builder_dsl_ref_type_narrowing_member calls this
/// automatically.
///
/// \code
///   .reference_type(std::type_index(typeid(int)), std::type_index(typeid(my_type)))
///   .reference_type(std::type_index(typeid(my_type))
/// \endcode
///
/// \paragraph serialization_builder_dsl_ref_formats_level_register_adapter register_adapter
///
///  - <tt>register_adapter(const adapter*)</tt>
///  - <tt>register_adapter(std::shared_ptr&lt;const adapter&gt;)</tt>
///
/// Register an arbitrary \c adapter with the \c formats we are currently building. This is useful for integrating with
/// type adapters that do not (or can not) use the DSL.
///
/// \code
///   .register_adapter(my_type::get_adapter())
/// \endcode
///
/// \paragraph serialization_builder_dsl_ref_formats_level_register_optional register_optional
///
///  - <tt>register_optional&lt;TOptional&gt;()</tt>
///
/// Similar to \c register_adapter, but automatically create an <tt>optional_adapter&lt;TOptional&gt;</tt> to store.
///
/// \code
///   .register_optional<std::optional<int>>()
///   .register_optional<boost::optional<double>>()
/// \endcode
///
/// \paragraph serialization_builder_dsl_ref_formats_level_register_container register_container
///
///  - <tt>register_container&lt;TContainer&gt;()</tt>
///
/// Similar to \c register_adapter, but automatically create a <tt>container_adapter&lt;TContainer&gt;</tt> to store.
///
/// \code
///   .register_container<std::vector<int>>()
///   .register_container<std::list<std::string>>()
/// \endcode
///
/// \paragraph serialization_builder_dsl_ref_formats_level_register_containers register_containers
///
///  - <tt>register_containers&lt;T, template &lt;class...&gt; class... TTContainer&gt;</tt>
///
/// Convenience function for calling \c register_container for multiple containers with the same \c value_type.
/// Unfortunately, it only supports varying the first template parameter of the \c TTContainer types, so if you wish to
/// do something like vary the allocator, you will have to either call \c register_container multiple times or use a
/// template alias.
///
/// \code
///   .register_containers<int, std::list, std::deque>()
///   .register_containers<double, std::vector, std::set>()
/// \endcode
///
/// \paragraph serialization_builder_dsl_ref_formats_level_register_wrapper register_wrapper
///
///  - <tt>register_wrapper&lt;TWrapper&gt;()</tt>
///
/// Similar to \c register_adapter, but automatically create an <tt>wrapper_adapter&lt;TWrapper&gt;</tt> to store.
///
/// \code
///   .register_optional<std::optional<int>>()
///   .register_optional<boost::optional<double>>()
/// \endcode
///
/// \paragraph serialization_builder_dsl_ref_formats_level_enum_type enum_type
///
///  - <tt>enum_type&lt;TEnum&gt;(std::string name, std::initializer_list&lt;std::pair&lt;TEnum, jsonv::value&gt;&gt;)</tt>
///  - <tt>enum_type_icase&lt;TEnum&gt;(std::string name, std::initializer_list&lt;std::pair&lt;TEnum, jsonv::value&gt;&gt;)</tt>
///
/// Create an adapter for the \c TEnum type with a mapping of C++ values to JSON values and vice versa. The most common
/// use of this is to map \c enum values in C++ to string representations in JSON. \c TEnum is not restricted to types
/// which are \c enum, but can be anything which you would like to restrict to a limited subset of possible values.
/// Likewise, JSON representations are not restricted to being of \c kind::string.
///
/// The sibling function \c enum_type_icase will create an adapter which uses case-insensitive checking when converting
/// to C++ values in \c extract.
///
/// \code
///   .enum_type<ring>("ring",
///                    {
///                      { ring::fire,  "fire"    },
///                      { ring::wind,  "wind"    },
///                      { ring::earth, "earth"   },
///                      { ring::water, "water"   },
///                      { ring::heart, "heart"   }, // "heart" is preferred for to_json
///                      { ring::heart, "useless" }, // "useless" is interpreted as ring::heart in extract
///                      { ring::fire,  1         }, // the JSON value 1 will also be interpreted as ring::fire in extract
///                      { ring::ussr,  "wind"    }, // old C++ value ring::ussr will get output as "wind"
///                    }
///                   )
///   .enum_type_icase<int>("integer",
///                         {
///                           { 0, "zero"   },
///                           { 0, "naught" },
///                           { 1, "one"    },
///                           { 2, "two"    },
///                           { 3, "three"  },
///                         }
///                        )
/// \endcode
///
/// \see enum_adapter
///
/// \paragraph serialization_builder_dls_ref_formats_level_polymorphic_type polymorphic_type
///
/// - <tt>polymorphic_type<&lt;TPointer&gt;(std::string discrimination_key);</tt>
///
/// Create an adapter for the \c TPointer type (usually \c std::shared_ptr or \c std::unique_ptr) that knows how to
/// serialize and deserialize one or more types that can be polymorphically represented by \c TPointer, i.e. derived
/// types. It uses a discrimination key to determine which concrete type should be instantiated when extracting values
/// from json.
///
/// \code
///   .polymorphic_type<std::unique_ptr<base>>("type")
///     .subtype<derived_1>("derived_1")
///     .subtype<derived_2>("derived_2", keyed_subtype_action::check)
///     .subtype<derived_3>("derived_3", keyed_subtype_action::insert);
/// \endcode
///
/// The \ref keyed_subtype_action can be used to configure the adapter to make sure that the discrimination key was
/// correctly serialized (\ref keyed_subtype_action::check) or to insert the discrimination key for the underlying type
/// so that the underlying type doesn't need to do that itself (\ref keyed_subtype_action::insert). The default is to do
/// nothing (\ref keyed_subtype_action::none).
///
/// \paragraph serialization_builder_dsl_ref_formats_level_extend extend
///
///  - <tt>extend(std::function&lt;void (formats_builder&amp;)&gt; func)</tt>
///
/// Extend the \c formats_builder with the provided \a func by passing the current builder to it. This provides a more
/// convenient way to call helper functions.
///
/// \code
/// jsonv::formats_builder builder;
/// foo(builder);
/// bar(builder);
/// baz(builder);
/// \endcode
///
/// This can be done equivalently with:
/// \code
/// jsonv::formats_builder()
///   .extend(foo)
///   .extend(bar)
///   .extend(baz)
/// \endcode
///
/// \paragraph serialization_builder_dsl_ref_formats_level_on_duplicate_type on_duplicate_type
///
///   - <tt>on_duplicate_type(on_duplicate_type_action action);</tt>
///
/// Set what action to take when attempting to register an adapter, but there is already an adapter for that type in the
/// formats. The default is to throw a \ref duplicate_type_error exception (\ref duplicate_type_action::exception), but
/// the \c formats_builder can also be configured to ignore the duplicate (\ref duplicate_type_action::ignore), or to
/// replace the existing adapter with the new one (\ref duplicate_type_action::replace). This is useful when calling
/// multiple \c extend methods that may add common types to the \c formats_builder.
///
/// \subsubsection serialization_builder_dsl_ref_formats_narrowing Narrowing
///
/// \paragraph serialization_builder_dsl_ref_formats_narrowing_type type&lt;T&gt;
///
///  - <tt>type&lt;T&gt;()</tt>
///  - <tt>type&lt;T&gt;(std::function&lt;void (adapter_builder&lt;T&gt;&amp;)&gt; func)</tt>
///
/// Create an \c adapter for type \c T and begin building the members for it. If \a func is provided, it will be called
/// with the adapter_builder&lt;T&gt; this call to \c type creates, which can be used for creating common extension
/// functions.
///
/// \code
///   .type<my_type>()
///       .member(...)
///       .
///       .
///       .
/// \endcode
///
///
/// \subsection serialization_builder_dsl_ref_type Type Context
///
/// Commands in this section modify the behavior of the \c jsonv::adapter for a particular type.
///
/// \subsubsection serialization_builder_dsl_ref_type_level Level
///
/// \paragraph serialization_builder_dsl_ref_type_level_pre_extract pre_extract
///
///  - <tt>pre_extract(std::function&lt;void (extraction_context& context)&gt; perform)</tt>
///
/// Call the given \a perform function during the \c extract operation, but before performing any extraction. This can
/// be called multiple times -- all functions will be called in the order they are provided.
///
/// The source document is not among the arguments. Extraction walks the reader forward, so at the point this runs
/// there is nothing read yet to hand over; see \ref serialization_builder_dsl_ref_type_level_post_extract
/// post_extract for a hook which sees the finished object.
///
/// \paragraph serialization_builder_dsl_ref_type_level_post_extract post_extract
///
///  - <tt>post_extract(std::function&lt;T (extraction_context& context, T&& out)&gt; perform)</tt>
///
/// Call the given \a perform function after the \c extract operation. All functions will be called in the order they
/// are provided. This allows validation methods to be called on the extracted object as part of extraction.
/// Postprocessing functions are allowed to mutate the extracted object.
///
/// \paragraph serialization_builder_dsl_ref_type_level_default_on_null type_default_on_null
///
///  - <tt>type_default_on_null()</tt>
///  - <tt>type_default_on_null(bool on)</tt>
///
/// If the JSON value \c null is in the input, should this type take on some default?
/// This should be used with \ref serialization_builder_dsl_ref_type_level_type_default_value type_default_value.
///
/// \paragraph serialization_builder_dsl_ref_type_level_type_default_value
///
///  - <tt>type_default_value(T value)</tt>
///  - <tt>type_default_value(std::function&lt;T (extraction_context& context)&gt;)</tt>
///
/// What value should be used to create the default for this type?
///
/// \code
///   .type<my_type>()
///       .type_default_on_null()
///       .type_default_value(my_type("default"))
/// \endcode
///
/// \paragraph serialization_builder_dsl_ref_type_level_on_extract_extra_keys on_extract_extra_keys
///
///  - <tt>on_extract_extra_keys(std::function&lt;void (extraction_context&      context,
///                                                   std::set&lt;std::string&gt; extra_keys)&gt; action
///                             )</tt>
///
/// When extracting, perform some \a action if extra keys are provided. By default, extra keys are usually simply
/// ignored, so this is useful if you wish to throw an exception (or anything you want). The \a action is handed the
/// names of the keys which claimed no member; their values have already been stepped over unread.
///
/// \code
///   .type<my_type>()
///       .member("x", &my_type::x)
///       .member("y", &my_type::y)
///       .on_extract_extra_keys([] (extraction_context&, std::set<std::string> extra_keys)
///                              {
///                                  throw extracted_extra_keys("my_type", std::move(extra_keys));
///                              }
///                             )
/// \endcode
///
/// There is a convenience function named \c throw_extra_keys_extraction_error which does this for you.
///
/// \code
///   .type<my_type>()
///       .member("x", &my_type::x)
///       .member("y", &my_type::y)
///       .on_extract_extra_keys(jsonv::throw_extra_keys_extraction_error)
/// \endcode
///
/// \subsubsection serialization_builder_dsl_ref_type_narrowing Narrowing
///
/// \paragraph serialization_builder_dsl_ref_type_narrowing_member member
///
///  - <tt>member(std::string name, TMember T::*selector)</tt>
///  - <tt>member(std::string name, const TMember& (*access)(const T&), void (*mutate)(T&, TMember&&))</tt>
///  - <tt>member(std::string name, const TMember& (T::*access)() const, TMember& (T::*mutable_access)())</tt>
///  - <tt>member(std::string name, const TMember& (T::*access)() const, void (T::*mutate)(TMember))</tt>
///  - <tt>member(std::string name, const TMember& (T::*access)() const, void (T::*mutate)(TMember&&))</tt>
///
/// Adds a member to the type we are currently building. By default, the member will be serialized with the key of the
/// given \a name and the extractor will search for the given \a name. If you wish to change properties of this field,
/// use the \ref serialization_builder_dsl_ref_member.
///
/// \code
///   .type<my_type>()
///       .member("x", &my_type::x)
///       .member("y", &my_type::y)
///       .member("thing", &my_type::get_thing, &my_type::set_thing)
/// \endcode
///
///
/// \subsection serialization_builder_dsl_ref_member Member Context
///
/// Commands in this section modify the behavior of a particular member. Here, \c T refers to the containing type (the
/// one we are adding a member to) and \c TMember refers to the type of the member we are modifying.
///
/// \subsubsection serialization_builder_dsl_ref_member_level Level
///
/// \paragraph serialization_builder_dsl_ref_member_level_after after
///
///  - <tt>after(version)</tt>
///
/// Only serialize this member if the \c serialization_context was not created with a version, or its version is
/// greater than the provided \c version.
///
/// \paragraph serialization_builder_dsl_ref_member_level_alternate_name alternate_name
///
///  - <tt>alternate_name(std::string name)</tt>
///
/// Provide an alternate name to search for when extracting this member. If a user provides values for multiple names,
/// preference is given to names earlier in the list, starting with the original given name.
///
/// \paragraph serialization_builder_dsl_ref_member_level_before before
///
///  - <tt>before(version)</tt>
///
/// Only serialize this member if the \c serialization_context was not created with a version, or its version is
/// less than the provided \c version.
///
/// \paragraph serialization_builder_dsl_ref_member_level_check_input check_input
///
///  - <tt>check_input(std::function&lt;void (const TMember&)&gt; check)</tt>
///  - <tt>check_input(std::function&lt;bool (const TMember&)&gt; check, std::function&lt;void (const TMember&)&gt; thrower)</tt>
///  - <tt>check_input(std::function&lt;bool (const TMember&)&gt; check, TException ex)</tt>
///
/// Checks the extracted value with the given \a check function. In the first form, you are expected to throw inside the
/// function. In the latter forms, the second parameter will be invoked (in the case of \a thrower) or thrown directly
/// (in the case of \a ex).
///
/// \code
///   .member("x", &my_type::x)
///       .check_input([] (int x) { if (x < 0) throw std::logic_error("x must be greater than 0"); })
///       .check_input([] (int x) { return x < 100; }, [] (int x) { throw exceptions::less_than(100, x); })
///       .check_input([] (int x) { return x % 2 == 0; }, std::logic_error("x must be divisible by 2"))
/// \endcode
///
/// \paragraph serialization_builder_dsl_ref_member_level_default_value default_value
///
///  - <tt>default_value(TMember value)</tt>
///  - <tt>default_value(std::function&lt;TMember (extraction_context& context)&gt; create)</tt>
///
/// Provide a default value for this member if no key is found when extracting. The function implementation can
/// synthesize the value however it likes, but it is not handed the object being extracted: a missing key is only known
/// to be missing once every key which was there has gone by, and the walk does not go back. A default which depends on
/// the other members belongs in \ref serialization_builder_dsl_ref_type_level_post_extract post_extract, which sees
/// the whole object once it is built.
///
/// \code
///  .member("x", &my_type::x)
///      .default_value(10)
/// \endcode
///
/// \paragraph serialization_builder_dsl_ref_member_level_default_on_null default_on_null
///
///  - <tt>default_on_null()</tt>
///  - <tt>default_on_null(bool on)</tt>
///
/// If the value associated with this key is \c kind::null, should that be treated as the default value? This option is
/// only considered if a \ref serialization_builder_dsl_ref_member_level_default_value default_value was provided.
///
/// \paragraph serialization_builder_dsl_ref_member_level_encode_if encode_if
///
///  - <tt>encode_if(std::function&lt;bool (const serialization_context&, const TMember&amp;)&gt; check)</tt>
///
/// Only serialize this member if the \a check function returns true.
///
/// \paragraph serialization_builder_dsl_ref_member_level_since since
///
///  - <tt>since(version)</tt>
///
/// Only serialize this member if the \c serialization_context was not created with a version, or its version is
/// greater than or equal to the provided \c version.
///
/// \paragraph serialization_builder_dsl_ref_member_level_until until
///
///  - <tt>until(version)</tt>
///
/// Only serialize this member if the \c serialization_context was not created with a version, or its version is
/// less than or equal to the provided \c version.

class formats_builder;

template <typename T> class adapter_builder;
template <typename T, typename TMember> class member_adapter_builder;
template <typename TPointer> class polymorphic_adapter_builder;

namespace detail
{

class formats_builder_dsl
{
public:
    explicit formats_builder_dsl(formats_builder* owner) :
            owner(owner)
    { }

    template <typename T>
    adapter_builder<T> type();

    template <typename T, typename F>
    adapter_builder<T> type(F&&);

    template <typename TEnum>
    formats_builder& enum_type(std::string enum_name, std::initializer_list<std::pair<TEnum, value>> mapping);

    template <typename TEnum>
    formats_builder& enum_type_icase(std::string enum_name, std::initializer_list<std::pair<TEnum, value>> mapping);

    template <typename TPointer>
    polymorphic_adapter_builder<TPointer> polymorphic_type(std::string discrimination_key = "");

    template <typename TPointer, typename F>
    polymorphic_adapter_builder<TPointer> polymorphic_type(std::string discrimination_key, F&&);

    template <typename F>
    formats_builder& extend(F&&);

    formats_builder& register_adapter(const adapter* p);
    formats_builder& register_adapter(std::shared_ptr<const adapter> p);

    formats_builder& reference_type(std::type_index typ);
    formats_builder& reference_type(std::type_index type, std::type_index from);

    template <typename TOptional>
    formats_builder& register_optional();

    template <typename TContainer>
    formats_builder& register_container();

    template <typename T, template <class...> class... TTContainers>
    formats_builder& register_containers();

    template <typename TWrapper>
    formats_builder& register_wrapper();

    formats_builder& check_references(const formats&       other,  const std::string& name = "");
    formats_builder& check_references(const formats::list& others, const std::string& name = "");
    formats_builder& check_references(const std::string& name = "");

    formats_builder& on_duplicate_type(duplicate_type_action action) noexcept;

    JSONV_NODISCARD
    formats compose_checked(formats other, const std::string& name = "");
    JSONV_NODISCARD
    formats compose_checked(std::vector<formats> others, const std::string& name = "");

    JSONV_NODISCARD
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

    adapter_builder<T>& type_default_on_null(bool on = true);

    adapter_builder<T>& type_default_value(std::function<T (extraction_context& ctx)> create);

    adapter_builder<T>& type_default_value(const T& value);

    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string name, TMember T::*selector);

    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string                              name,
                                              std::function<const TMember& (const T&)> access,
                                              std::function<void (T&, TMember&&)>      mutate
                                             );

    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string    name,
                                              const TMember& (T::*access)() const,
                                              TMember&       (T::*mutable_access)()
                                             );

    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string name,
                                              const TMember& (T::*access)() const,
                                              void (T::*mutate)(TMember)
                                             );

    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string name,
                                              const TMember& (T::*access)() const,
                                              void (T::*mutate)(TMember&&)
                                             );

    adapter_builder<T>& pre_extract(typename adapter_builder<T>::pre_extract_func perform);

    adapter_builder<T>& post_extract(typename adapter_builder<T>::post_extract_func perform);

    adapter_builder<T>& on_extract_extra_keys(typename adapter_builder<T>::extra_keys_func handler);

protected:
    adapter_builder<T>* owner;
};

/// Is the value \a from is positioned on a JSON `null`?
///
/// Asked of the lent \c value where there is one, exactly as \c optional_adapter does and for the same reason: a
/// value-backed reader has no token for a non-finite \c kind::decimal and renders one as \c literal_null, so going by
/// the node type alone would call a \c double holding a NaN "null" and take a default for it.
JSONV_NODISCARD
inline bool current_is_null(const reader& from)
{
    if (const value* lent = from.current_value())
        return lent->kind() == jsonv::kind::null;
    else
        return from.good() && from.current().type() == ast_node_type::literal_null;
}

/// The answer \c member_adapter::extract_key_rank gives for a key which names no member.
constexpr std::size_t no_extract_key = std::size_t(-1);

/// Which member each key has claimed, and by which of that member's names.
///
/// A member answers to the name it was declared with and to every \c alternate_name after it, and that list is a
/// preference order. Remembering only *that* a member was claimed cannot tell a second spelling of it from a repeat
/// of the first, which is a different question with a different answer: the first is the document naming one member
/// two ways, where the earliest name wins, and the second is a duplicate key, which is
/// \c extract_options::on_duplicate_key's to decide.
///
/// Read once the walk is done, so it has to be cheap to make: a wholly successful extraction of an object allocates
/// nothing, and a `std::vector` would spoil that for every object extracted. Types with more members than fit inline
/// fall back to one.
class member_claim_set
{
public:
    /// No key has claimed this member.
    static constexpr std::uint16_t unclaimed = std::uint16_t(-1);

public:
    explicit member_claim_set(std::size_t count)
    {
        if (count > inline_capacity)
        {
            _spilled = std::make_unique<std::uint16_t[]>(count);
            std::fill_n(_spilled.get(), count, unclaimed);
        }
        else
        {
            _ranks.fill(unclaimed);
        }
    }

    /// Which of member \a idx's names claimed it, or \ref unclaimed if none has.
    JSONV_NODISCARD
    std::uint16_t claim_rank(std::size_t idx) const
    {
        return _spilled ? _spilled[idx] : _ranks[idx];
    }

    JSONV_NODISCARD
    bool claimed(std::size_t idx) const
    {
        return claim_rank(idx) != unclaimed;
    }

    /// Record that the name at \a rank is the one member \a idx is being read from.
    void claim(std::size_t idx, std::size_t rank)
    {
        // A member with more names than this can count is not something anyone builds; the clamp is here so the type
        // can stay narrow, not because the case is expected.
        auto stored = std::uint16_t(std::min<std::size_t>(rank, unclaimed - 1U));

        if (_spilled)
            _spilled[idx] = stored;
        else
            _ranks[idx] = stored;
    }

private:
    static constexpr std::size_t inline_capacity = 64U;

    /// A plain array rather than a `std::vector`, which allocates a debugging proxy on some standard libraries even
    /// when it is empty -- and this one is empty for every type small enough to be tracked inline, which is nearly
    /// all of them.
    std::array<std::uint16_t, inline_capacity> _ranks;
    std::unique_ptr<std::uint16_t[]>           _spilled;
};

template <typename T>
class member_adapter
{
public:
    virtual ~member_adapter() noexcept
    { }

    /// Extract this member from \a from, which is positioned on the member's value, and set it on \a out. On return
    /// the cursor sits one position past that value, as every extractor owes its caller.
    ///
    /// \param key The key the document used, which is what names this member in a problem raised inside it -- the
    ///            declared name would be the wrong answer for a member matched through an \c alternate_name. It must
    ///            outlive the call, which the caller arranges: a canonical key views the source, and an escaped one
    ///            views the \c std::string the key loop decoded it into.
    JSONV_NODISCARD
    virtual std::expected<void, ast_node_type>
    extract(extraction_context& context, reader& from, std::string_view key, T& out) const = 0;

    /// Apply this member's default to \a out, reading nothing. Called when no key claimed this member, and when the
    /// key which did held \c null and \c default_on_null is set.
    JSONV_NODISCARD
    virtual std::expected<void, ast_node_type> apply_default(extraction_context& context, T& out) const = 0;

    virtual void to_json(const serialization_context& context, const T& from, value& out) const = 0;

    /// Does this member have a default to fall back on? A member without one is required.
    JSONV_NODISCARD
    virtual bool has_default() const = 0;

    /// Where \a key sits in this member's list of names -- 0 for the name it was declared with, then one for each
    /// \c alternate_name in the order they were added -- or \ref no_extract_key if this member does not answer to it.
    ///
    /// That position is the preference order `alternate_name` documents, and a forward walk has to enforce it for
    /// itself: it meets the names in the order the *document* put them, which says nothing about which one the type
    /// prefers.
    JSONV_NODISCARD
    virtual std::size_t extract_key_rank(std::string_view key) const = 0;

    /// The name this member was declared with, for naming it in a problem raised where the document's own key is not
    /// to hand. It is owned by this adapter and outlives any extraction, so it is safe to push as a `path_scope`.
    JSONV_NODISCARD
    virtual std::string_view primary_name() const = 0;
};

template <typename T, typename TMember>
class member_adapter_impl :
        public member_adapter<T>
{
public:
    using mutator_type  = std::function<void (T&, TMember&&)>;
    using accessor_type = std::function<const TMember& (const T&)>;

public:
    explicit member_adapter_impl(std::string name, mutator_type mutator, accessor_type access) :
            _names({ std::move(name) }),
            _set_value(std::move(mutator)),
            _get_value(std::move(access))
    { }

    explicit member_adapter_impl(std::string name, TMember T::*selector) :
            member_adapter_impl(std::move(name),
                                [selector] (T& value, TMember&& x) { value.*selector = std::move(x); },
                                [selector] (const T& value) -> const TMember& { return value.*selector; }
                               )
    { }

    JSONV_NODISCARD
    virtual std::expected<void, ast_node_type>
    extract(extraction_context& context, reader& from, std::string_view key, T& out) const override
    {
        if (_default_on_null && current_is_null(from))
        {
            (void) from.next_token();
            return apply_default(context, out);
        }

        // Scoped to the extraction and not to the assignment. `_set_value` is whatever the
        // `member(name, access, mutate)` overload was handed, so it is arbitrary user code, and once the member's
        // value exists the extractor is no longer at this key -- something that setter goes on to extract is where
        // the document says it is rather than underneath this member.
        //
        // `key` is the key the document actually used, which is the one to name when a member matched through an
        // `alternate_name`. The caller keeps it alive across this call, so naming it costs nothing.
        auto extracted = [&] () -> std::expected<TMember, ast_node_type>
                         {
                             extraction_context::path_scope scope(context, key);

                             return context.extract<TMember>(from);
                         }();

        if (!extracted)
            return std::unexpected(extracted.error());

        // `check_input` runs on the value which was read, before it reaches the setter. It throws rather than
        // reporting, so it is user code the loop above has to be ready for.
        if (_extract_mutate)
            _set_value(out, _extract_mutate(*std::move(extracted)));
        else
            _set_value(out, *std::move(extracted));

        return {};
    }

    JSONV_NODISCARD
    virtual std::expected<void, ast_node_type> apply_default(extraction_context& context, T& out) const override
    {
        _set_value(out, _default_value(context));
        return {};
    }

    virtual void to_json(const serialization_context& context, const T& from, value& out) const override
    {
        if (should_encode(context, from))
            out.insert({ _names.at(0), context.to_json(_get_value(from)) });
    }

    JSONV_NODISCARD
    virtual bool has_default() const override
    {
        return bool(_default_value);
    }

    JSONV_NODISCARD
    virtual std::size_t extract_key_rank(std::string_view key) const override
    {
        for (std::size_t rank = 0U; rank < _names.size(); ++rank)
            if (_names[rank] == key)
                return rank;

        return no_extract_key;
    }

    JSONV_NODISCARD
    virtual std::string_view primary_name() const override
    {
        return _names.at(0);
    }

    void add_encode_check(std::function<bool (const serialization_context&, const TMember&)> check)
    {
        if (_should_encode)
        {
            auto old_check = std::move(_should_encode);
            _should_encode = [check, old_check] (const serialization_context& context, const TMember& value)
                             {
                                return check(context, value) && old_check(context, value);
                             };
        }
        else
        {
            _should_encode = std::move(check);
        }
    }

    void add_extraction_mutator(std::function <TMember (TMember&&)> mutate)
    {
        if (_extract_mutate)
        {
            auto old_mutate = std::move(_extract_mutate);
            _extract_mutate = [old_mutate, mutate] (TMember&& member) { return mutate(old_mutate(std::move(member))); };
        }
        else
        {
            _extract_mutate = std::move(mutate);
        }
    }

    void add_extraction_check(std::function <void (const TMember&)> check)
    {
        add_extraction_mutator([check] (TMember&& value)
        {
            check(value);
            return value;
        });
    }

    void default_value(std::function<TMember (extraction_context&)>&& create)
    {
        _default_value = std::move(create);
    }

    void default_on_null(bool on)
    {
        _default_on_null = on;
    }

private:
    bool should_encode(const serialization_context& context, const T& from) const
    {
        if (_should_encode)
            return _should_encode(context, _get_value(from));
        else
            return true;
    }

private:
    // Qualified: unqualified, this declares a friend `jsonv::detail::member_adapter_builder`, which is a different
    // (and nonexistent) template from the `jsonv::member_adapter_builder` which actually reaches in here. Every use
    // of `alternate_name` failed to compile until this was spelled out.
    template <typename U, typename UMember>
    friend class jsonv::member_adapter_builder;

private:
    std::vector<std::string>                                           _names;
    mutator_type                                                       _set_value;
    accessor_type                                                      _get_value;
    std::function<bool (const serialization_context&, const TMember&)> _should_encode;
    std::function<TMember (extraction_context&)>                       _default_value;
    bool                                                               _default_on_null = false;
    std::function<TMember (TMember&&)>                                 _extract_mutate;
};

}

template <typename T, typename TMember>
class member_adapter_builder :
        public detail::formats_builder_dsl,
        public detail::adapter_builder_dsl<T>
{
public:
    explicit member_adapter_builder(formats_builder*                         fmt_builder,
                                    adapter_builder<T>*                      adapt_builder,
                                    detail::member_adapter_impl<T, TMember>* adapter
                                   ) :
            formats_builder_dsl(fmt_builder),
            detail::adapter_builder_dsl<T>(adapt_builder),
            _adapter(adapter)
    {
        reference_type(std::type_index(typeid(TMember)), std::type_index(typeid(T)));
    }

    /** When extracting, also look for this \a name as a key. **/
    member_adapter_builder& alternate_name(std::string name)
    {
        _adapter->_names.emplace_back(std::move(name));
        return *this;
    }

    member_adapter_builder& check_input(std::function<void (const TMember&)> check)
    {
        _adapter->add_extraction_check(std::move(check));
        return *this;
    }

    member_adapter_builder& check_input(std::function<bool (const TMember&)> check,
                                        std::function<void (const TMember&)> thrower
                                       )
    {
        _adapter->add_extraction_check([check, thrower] (const TMember& value)
            {
                if (!check(value))
                    thrower(value);
            });
        return *this;
    }

    template <typename TException>
    member_adapter_builder& check_input(std::function<void (const TMember&)> check, const TException& ex)
    {
        return check_input(std::move(check), [ex] (const TMember&) { throw ex; });
    }

    /** If the key for this member is not in the object when deserializing, call this function to create a value. If a
     *  \c default_value is not specified, the key is required.
    **/
    member_adapter_builder& default_value(std::function<TMember (extraction_context&)> create)
    {
        _adapter->default_value(std::move(create));
        return *this;
    }

    /** If the key for this member is not in the object when deserializing, use this \a value. If a \c default_value is
     *  not specified, the key is required.
    **/
    member_adapter_builder& default_value(TMember value)
    {
        return default_value([value] (extraction_context&) { return value; });
    }

    /** Should a \c kind::null for a key be interpreted as a missing value? **/
    member_adapter_builder& default_on_null(bool on = true)
    {
        _adapter->default_on_null(on);
        return *this;
    }

    /** Only encode this member if the \a check passes. The final decision to encode is based on \e all \c check
     *  functions.
    **/
    member_adapter_builder& encode_if(std::function<bool (const serialization_context&, const TMember&)> check)
    {
        _adapter->add_encode_check(std::move(check));
        return *this;
    }

    /// Only encode this member if the \c serialization_context was not created with a version, or its version
    /// is greater than or equal to \a ver.
    member_adapter_builder& since(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const TMember&)
                         {
                             return !context.version() || *context.version() >= ver;
                         }
                        );
    }

    /// Only encode this member if the \c serialization_context was not created with a version, or its version
    /// is less than or equal to \a ver.
    member_adapter_builder& until(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const TMember&)
                         {
                             return !context.version() || *context.version() <= ver;
                         }
                        );
    }

    /// Only encode this member if the \c serialization_context was not created with a version, or its version
    /// is greater than \a ver.
    member_adapter_builder& after(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const TMember&)
                         {
                             return !context.version() || *context.version() > ver;
                         }
                        );
    }

    /// Only encode this member if the \c serialization_context was not created with a version, or its version
    /// is less than \a ver.
    member_adapter_builder& before(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const TMember&)
                         {
                             return !context.version() || *context.version() < ver;
                         }
                        );
    }

private:
    detail::member_adapter_impl<T, TMember>* _adapter;
};

template <typename T>
class adapter_builder :
        public detail::formats_builder_dsl
{
public:
    using pre_extract_func  = std::function<void (extraction_context&)>;
    using post_extract_func = std::function<T (extraction_context&, T&&)>;
    using extra_keys_func   = std::function<void (extraction_context&, std::set<std::string>)>;

public:
    template <typename F>
    explicit adapter_builder(formats_builder* owner, F&& f) :
            formats_builder_dsl(owner),
            _adapter(nullptr)
    {
        auto adapter = std::make_shared<adapter_impl>();
        register_adapter(adapter);
        _adapter = adapter.get();

        std::forward<F>(f)(*this);
    }

    explicit adapter_builder(formats_builder* owner) :
            adapter_builder(owner, [] (const adapter_builder<T>&) { })
    { }

    adapter_builder<T>& type_default_on_null(bool on = true)
    {
        _adapter->_default_on_null = on;
        return *this;
    }

    adapter_builder<T>& type_default_value(std::function<T (extraction_context& ctx)> create)
    {
        _adapter->_create_default = std::move(create);
        return *this;
    }

    adapter_builder<T>& type_default_value(const T& value)
    {
        return type_default_value([value] (extraction_context&) { return T(value); });
    }

    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string name, TMember T::*selector)
    {
        std::unique_ptr<detail::member_adapter_impl<T, TMember>> ptr
            (
                new detail::member_adapter_impl<T, TMember>(std::move(name), selector)
            );
        member_adapter_builder<T, TMember> builder(formats_builder_dsl::owner, this, ptr.get());
        _adapter->_members.emplace_back(std::move(ptr));
        return builder;
    }

    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string                              name,
                                              std::function<const TMember& (const T&)> access,
                                              std::function<void (T&, TMember&&)>      mutate
                                             )
    {
        std::unique_ptr<detail::member_adapter_impl<T, TMember>> ptr
            (
                new detail::member_adapter_impl<T, TMember>(std::move(name), std::move(mutate), std::move(access))
            );
        member_adapter_builder<T, TMember> builder(formats_builder_dsl::owner, this, ptr.get());
        _adapter->_members.emplace_back(std::move(ptr));
        return builder;
    }

    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string    name,
                                              const TMember& (T::*access)() const,
                                              TMember&       (T::*mutable_access)()
                                             )
    {
        return member<TMember>(std::move(name),
                               access,
                               [mutable_access] (T& x, TMember&& val) { (x.*mutable_access)() = std::move(val); }
                              );
    }

    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string name,
                                              const TMember& (T::*access)() const,
                                              void (T::*mutate)(TMember)
                                             )
    {
        return member<TMember>(std::move(name),
                               std::function<const TMember& (const T&)>(access),
                               [mutate] (T& x, TMember val) { (x.*mutate)(std::move(val)); }
                              );
    }

    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string name,
                                              const TMember& (T::*access)() const,
                                              void (T::*mutate)(TMember&&)
                                             )
    {
        return member<TMember>(std::move(name),
                               std::function<const TMember& (const T&)>(access),
                               std::function<void (T&, TMember&&)>(mutate)
                              );
    }

    adapter_builder<T>& pre_extract(pre_extract_func perform)
    {
        if (_adapter->_pre_extract)
        {
            pre_extract_func old_perform = std::move(_adapter->_pre_extract);
            _adapter->_pre_extract = [old_perform, perform] (extraction_context& context)
                                     {
                                         old_perform(context);
                                         perform(context);
                                     };
        }
        else
        {
            _adapter->_pre_extract = std::move(perform);
        }
        return *this;
    }

    adapter_builder<T>& post_extract(post_extract_func perform)
    {
        if (_adapter->_post_extract)
        {
            post_extract_func old_perform = std::move(_adapter->_post_extract);
            _adapter->_post_extract = [old_perform, perform] (extraction_context& context, T&& out)
                                     {
                                         return perform(context, old_perform(context, std::move(out)));
                                     };
        }
        else
        {
            _adapter->_post_extract = std::move(perform);
        }
        return *this;
    }

    /// The handler is stored rather than desugared into a \c pre_extract, because the keys which claimed no member
    /// are only known once the walk is done. It is still registered against the members as they stand at extraction
    /// time rather than at build time -- the key loop does the matching -- so declaring it before the members it
    /// validates against keeps working.
    adapter_builder<T>& on_extract_extra_keys(extra_keys_func handler)
    {
        if (_adapter->_extra_keys)
        {
            extra_keys_func old_handler = std::move(_adapter->_extra_keys);
            _adapter->_extra_keys = [old_handler, handler] (extraction_context& context, std::set<std::string> keys)
                                    {
                                        old_handler(context, keys);
                                        handler(context, std::move(keys));
                                    };
        }
        else
        {
            _adapter->_extra_keys = std::move(handler);
        }
        return *this;
    }

private:
    class adapter_impl :
            public adapter_for<T>
    {
    public:
        adapter_impl() :
                _default_on_null(false)
        { }

        JSONV_NODISCARD
        virtual std::expected<T, ast_node_type> create(extraction_context& context, reader& from) const override
        {
            if (_pre_extract)
                _pre_extract(context);

            if (_default_on_null && detail::current_is_null(from))
            {
                (void) from.next_token();

                try
                {
                    return _create_default(context);
                }
                catch (...)
                {
                    // The `null` this stands in for is already behind the cursor, so whatever recovers from this
                    // must not step over the value after it as well. Both the factory and the move of its result
                    // into the answer are the caller's code.
                    context.note_value_consumed(from);
                    throw;
                }
            }

            auto opened = context.current_as<ast_node::object_begin>(from);
            if (!opened)
                return std::unexpected(opened.error());

            T                        out;
            detail::member_claim_set claims(_members.size());
            bool                     recovered = false;
            bool                     closed    = false;

            // Both of these stay empty on the ordinary path and are built only where they are actually wanted. A
            // default-constructed container is not free everywhere -- some standard libraries allocate a debugging
            // proxy or an end sentinel for one -- and this runs once per object extracted.
            std::optional<std::set<std::string>> extra_keys;
            std::optional<std::set<std::string>> repeated_keys;

            // `duplicate_key_action::exception` is the one policy which needs to know the keys themselves rather
            // than which member they claimed. A member's winning name cannot tell a lower-ranked name it is skipping
            // from one it has already skipped, so which of the two an object is refused for would otherwise depend
            // on the order it happened to list them in.
            if (context.options().on_duplicate_key() == extract_options::duplicate_key_action::exception)
                repeated_keys.emplace();

            // Step off the `{` and onto the first key, or onto the `}` of an empty object. The loop never advances
            // itself: extracting a member leaves the cursor one past its value, which is what every extractor owes
            // its caller.
            (void) from.next_token();

            try
            {
                while (from.good())
                {
                    auto type = from.current().type();

                    if (type == ast_node_type::document_end || type == ast_node_type::error)
                    {
                        // A parse which failed part-way through an object still hands back a usable tape; it just
                        // ends where the rest of the members should have been.
                        return context.problem(context.problem_path(from), "Unterminated object");
                    }
                    else if (type == ast_node_type::object_end)
                    {
                        // Deliberately *not* stepped over. Everything after this loop runs user code, and while the
                        // cursor is on a closing token the reader still names this object rather than the sibling
                        // after it -- which is both where a failure out of that code belongs and where a caller
                        // recovering from it resumes, since `reader::next_value` on a `}` is a single step past it.
                        closed = true;
                        break;
                    }

                    // A canonical key is a view of the source, which outlives this whole extraction. An escaped one
                    // has to be decoded, and it is decoded here rather than inside the member because dispatching it
                    // needs the text anyway; `decoded` owns it for as long as a problem naming it can be raised.
                    std::optional<std::string> decoded;
                    std::string_view           key;
                    if (type == ast_node_type::key_canonical)
                    {
                        key = from.current().as<ast_node::key_canonical>().value();
                    }
                    else if (type == ast_node_type::key_escaped)
                    {
                        key = decoded.emplace(from.current().as<ast_node::key_escaped>().value());
                    }
                    else
                    {
                        auto matched = context.expect(from,
                                                      { ast_node_type::key_canonical, ast_node_type::key_escaped }
                                                     );
                        return std::unexpected(matched.error());
                    }

                    if (!from.next_token())
                        return context.problem(context.problem_path(from), "Unterminated object");

                    if (repeated_keys && !repeated_keys->emplace(key).second)
                    {
                        // Asked of every key rather than only of the ones a member answers to, since a key repeated
                        // twice is the same thing to this policy whether anything claimed it or not -- and it is
                        // what `parse_index::extract_tree` refuses for the same document.
                        std::string message("Duplicate key in object: \"");
                        message.append(key);
                        message.append("\"");

                        (void) context.problem(context.path(), std::move(message));

                        if (!context.recover())
                            return std::unexpected(ast_node_type::error);

                        recovered = true;
                        (void) from.next_value();
                        continue;
                    }

                    auto matched = find_member(key);
                    if (matched.index == _members.size())
                    {
                        // Nothing claimed it. The whole subtree goes by in constant time on a tape-backed reader,
                        // and its name is only worth keeping if somebody registered to be told about it.
                        if (_extra_keys)
                        {
                            if (!extra_keys)
                                extra_keys.emplace();

                            extra_keys->emplace(key);
                        }

                        (void) from.next_value();
                        continue;
                    }

                    if (auto held = claims.claim_rank(matched.index); held != detail::member_claim_set::unclaimed)
                    {
                        if (matched.rank > held)
                        {
                            // Another of this member's names, but one it already answers to by a better one. That is
                            // the document naming a member two ways rather than repeating a key, so the duplicate
                            // policy has nothing to say about it and the value goes by unread -- which is also what
                            // keeps a `check_input` on the member from being run against a spelling it did not take.
                            (void) from.next_value();
                            continue;
                        }
                        else if (matched.rank == held && !consume_duplicate(context, from))
                        {
                            // The same name twice, which is what `duplicate_key_action` is about.
                            continue;
                        }

                        // Otherwise a name the member prefers to the one it is being read from, which supersedes it.
                    }

                    const auto& member = *_members[matched.index];
                    auto        walked = run_member(context,
                                                    member,
                                                    &from,
                                                    [&] { return member.extract(context, from, key, out); }
                                                   );

                    // Claimed even when it failed: the key was there, so the pass below has nothing to say about it.
                    claims.claim(matched.index, matched.rank);

                    if (!walked)
                        return std::unexpected(walked.error());
                    else if (!*walked)
                        recovered = true;
                }

                if (!closed)
                    return context.problem(context.problem_path(from), "Unterminated object");

                // Reported before the members which never arrived, because that is the order the extra-key handler
                // used to run in when it was a `pre_extract` -- ahead of everything the walk itself has to say.
                if (_extra_keys && extra_keys && !extra_keys->empty())
                    _extra_keys(context, *std::move(extra_keys));

                for (std::size_t idx = 0U; idx < _members.size(); ++idx)
                {
                    if (claims.claimed(idx))
                        continue;

                    const auto& member = *_members[idx];
                    if (!member.has_default())
                    {
                        std::string message("Missing required field ");
                        message.append(member.primary_name());

                        (void) context.problem(context.path(), std::move(message));
                        if (!context.recover())
                            return std::unexpected(ast_node_type::error);

                        recovered = true;
                        continue;
                    }

                    // The factory reads nothing, so there is no value in front of the cursor for a recovery to step
                    // over -- hence no reader.
                    auto applied = run_member(context,
                                              member,
                                              nullptr,
                                              [&] { return member.apply_default(context, out); }
                                             );

                    if (!applied)
                        return std::unexpected(applied.error());
                    else if (!*applied)
                        recovered = true;
                }
            }
            catch (...)
            {
                // Something left the walk: a setter, a `check_input` or a default factory, all of which are the
                // caller's code. Unless this object's `}` was reached the cursor is somewhere inside it, a position
                // only this adapter can make sense of, so finish the walk before letting the failure out. A loop
                // above which recovers resumes at the `}`, exactly as it would from a returned failure.
                if (!closed)
                    walk_to_close(from);

                throw;
            }

            // Collecting gathers diagnostics; it does not make a half-populated `out` worth handing back. Reported
            // before `_post_extract`, which has no business seeing one.
            if (recovered)
                return std::unexpected(ast_node_type::error);

            if (_post_extract)
                out = _post_extract(context, std::move(out));

            // Only now, once nothing left is able to fail with this object in front of the cursor.
            (void) from.next_token();

            try
            {
                return out;
            }
            catch (...)
            {
                // Moving `out` into the answer is the last thing which can fail and `T` is the caller's type. The
                // object is behind the cursor by this point, where every other failure in here leaves it in front.
                context.note_value_consumed(from);
                throw;
            }
        }

        virtual value to_json(const serialization_context& context, const T& from) const override
        {
            value out = object();
            for (const auto& member : _members)
                member->to_json(context, from, out);
            return out;
        }


        std::deque<std::unique_ptr<detail::member_adapter<T>>> _members;
        pre_extract_func                                       _pre_extract;
        post_extract_func                                      _post_extract;
        extra_keys_func                                        _extra_keys;
        std::function<T (extraction_context&)>                 _create_default;
        bool                                                   _default_on_null;

    private:
        /// Which member \a key claimed, and by which of that member's names.
        struct member_match
        {
            /// The member, or \c _members.size() when the key claimed none.
            std::size_t index;

            /// Which of its names matched, lower being preferred; \c detail::no_extract_key when none did.
            std::size_t rank;
        };

        /// Find the member which claims \a key.
        ///
        /// A linear scan of the members, each scanning its own names. For the member counts this DSL is used with
        /// that is cheaper than anything with a hash in it.
        JSONV_NODISCARD
        member_match find_member(std::string_view key) const
        {
            for (std::size_t idx = 0U; idx < _members.size(); ++idx)
                if (auto rank = _members[idx]->extract_key_rank(key); rank != detail::no_extract_key)
                    return member_match{ idx, rank };

            return member_match{ _members.size(), detail::no_extract_key };
        }

        /// Deal with \a key naming a member some earlier key already set, with the cursor on the repeat's value.
        ///
        /// \returns \c true to go on and extract it over the top of what is there, which is
        ///          \c extract_options::duplicate_key_action::replace; \c false when the value has been stepped over
        ///          and the walk should move on to the next key.
        ///
        /// \c duplicate_key_action::exception is not decided here. It is answered for every key on the way past,
        /// because it is the only policy which cares about a repeat this member would otherwise never be told
        /// about -- a second helping of a name it has already passed over for a better one.
        JSONV_NODISCARD
        static bool consume_duplicate(extraction_context& context, reader& from)
        {
            if (context.options().on_duplicate_key() == extract_options::duplicate_key_action::ignore)
            {
                (void) from.next_value();
                return false;
            }

            return true;
        }

        /// Run one member's part of the walk and fold however it fails into the channel the loop deals in.
        ///
        /// Extraction reports failure by returning, but a member reaches the caller's code in three places -- the
        /// setter, a \c check_input and a default factory -- and those report by throwing whatever they like. Giving
        /// them the same shape is what keeps which members get attempted from depending on how the first failing one
        /// happened to fail.
        ///
        /// \param from The reader to resume from, or \c nullptr where \a run reads nothing. A member which failed by
        ///             returning still has the value it rejected in front of the cursor; one which failed by throwing
        ///             has already stepped over it, and a default factory never had one.
        /// \returns \c true when \a run succeeded and \c false when it failed and was recovered from, in which case
        ///          the walk continues from where the cursor stands; otherwise the failure to report.
        template <typename FRun>
        JSONV_NODISCARD
        static std::expected<bool, ast_node_type> run_member(extraction_context&              context,
                                                             const detail::member_adapter<T>& member,
                                                             reader*                          from,
                                                             FRun&&                           run
                                                            )
        {
            try
            {
                if (auto result = run())
                {
                    return true;
                }
                else if (!context.recover())
                {
                    return std::unexpected(result.error());
                }
                else if (from)
                {
                    context.skip_failed_value(*from);
                }
            }
            catch (const extraction_error& ex)
            {
                // The next key starts at a known place, so one bad member does not have to hide every problem after
                // it.
                if (!context.recover(ex))
                    throw;
            }
            catch (const std::bad_alloc&)
            {
                // Recovering means recording, and recording allocates -- as does the `extraction_error` below, whose
                // constructors are `noexcept`, so failing to allocate inside one terminates rather than propagating.
                // There is nothing to be gained by trying.
                throw;
            }
            catch (...)
            {
                // Asked before anything is built, because translating allocates and rethrowing the original
                // untouched is what leaves `fail_immediately` reaching the same translation it always did.
                if (context.options().failure_mode() != extract_options::on_error::collect_all)
                    throw;

                // `member_adapter::extract` names the member it failed in through the key the document used; this
                // failure happened outside that -- or, for a default factory, with no key to name it by at all --
                // and would otherwise be the one problem in the list which does not say where it came from.
                // Unwinding completes before a handler body runs, so any scope the member pushed is long gone by the
                // time this one does and there is nothing to double up with.
                extraction_context::path_scope scope(context, member.primary_name());
                extraction_error               translated(context.path(), std::current_exception());

                if (!context.recover(translated))
                    throw;
            }

            return false;
        }

        /// Walk \a from to this object's own closing token, leaving the cursor on it.
        ///
        /// Stepping over whole member values is what finds it: \c reader::next_structure cannot, because a member
        /// which is itself a structure gets left rather than crossed, landing back inside the object being built.
        static void walk_to_close(reader& from) noexcept
        {
            while (from.good())
            {
                auto type = from.current().type();
                if (  type == ast_node_type::object_end
                   || type == ast_node_type::document_end
                   || type == ast_node_type::error
                   )
                {
                    break;
                }
                else if (type == ast_node_type::key_canonical || type == ast_node_type::key_escaped)
                {
                    // From a key, this is the whole member -- the key and the value under it.
                    (void) from.next_key();
                }
                else
                {
                    (void) from.next_value();
                }
            }
        }
    };

private:
    adapter_impl* _adapter;
};

template <typename TPointer>
class polymorphic_adapter_builder :
        public detail::formats_builder_dsl
{
public:
    template <typename F>
    explicit polymorphic_adapter_builder(formats_builder* owner,
                                         std::string      discrimination_key,
                                         F&&              f
                                        ) :
            formats_builder_dsl(owner),
            _adapter(nullptr),
            _discrimination_key(std::move(discrimination_key))
    {
        auto adapter = std::make_shared<polymorphic_adapter<TPointer>>();
        register_adapter(adapter);
        _adapter = adapter.get();

        std::forward<F>(f)(*this);
    }

    explicit polymorphic_adapter_builder(formats_builder* owner, std::string discrimination_key = "") :
            polymorphic_adapter_builder(owner,
                                        std::move(discrimination_key),
                                        [] (const polymorphic_adapter_builder<TPointer>&) { }
                                       )
    { }

    polymorphic_adapter_builder& check_null_input(bool on = true)
    {
        _adapter->check_null_input(on);
        return *this;
    }

    polymorphic_adapter_builder& check_null_output(bool on = true)
    {
        _adapter->check_null_output(on);
        return *this;
    }

    template <typename TSub>
    polymorphic_adapter_builder& subtype(value discrimination_value,
                                         keyed_subtype_action action = keyed_subtype_action::none)
    {
        if (_discrimination_key.empty())
            throw std::logic_error("Cannot use single-argument subtype if no discrimination_key has been set");

        return subtype<TSub>(_discrimination_key, std::move(discrimination_value), action);
    }

    template <typename TSub>
    polymorphic_adapter_builder& subtype(std::string discrimination_key,
                                         value discrimination_value,
                                         keyed_subtype_action action = keyed_subtype_action::none)
    {
        _adapter->template add_subtype_keyed<TSub>(std::move(discrimination_key),
                                                   std::move(discrimination_value),
                                                   action);
        reference_type(std::type_index(typeid(TSub)), std::type_index(typeid(TPointer)));
        return *this;
    }

    template <typename TSub>
    polymorphic_adapter_builder& subtype(std::function<bool (extraction_context&, const value&)> discriminator)
    {
        _adapter->template add_subtype<TSub>(std::move(discriminator));
        reference_type(std::type_index(typeid(TSub)), std::type_index(typeid(TPointer)));
        return *this;
    }

    template <typename TSub>
    polymorphic_adapter_builder& subtype(std::function<bool (const value&)> discriminator)
    {
        return subtype<TSub>([discriminator] (extraction_context&, const value& val)
                             {
                                 return discriminator(val);
                             }
                            );
    }

private:
    polymorphic_adapter<TPointer>* _adapter;
    std::string                    _discrimination_key;
};

class JSONV_PUBLIC formats_builder
{
public:
    formats_builder();

    template <typename T>
    adapter_builder<T> type()
    {
        return adapter_builder<T>(this);
    }

    template <typename T, typename F>
    adapter_builder<T> type(F&& f)
    {
        return adapter_builder<T>(this, std::forward<F>(f));
    }

    template <typename TEnum>
    formats_builder& enum_type(std::string                                    enum_name,
                               std::initializer_list<std::pair<TEnum, value>> mapping
                              )
    {
        return register_adapter(std::make_shared<enum_adapter<TEnum>>(std::move(enum_name), mapping));
    }

    template <typename TEnum>
    formats_builder& enum_type_icase(std::string                                    enum_name,
                                     std::initializer_list<std::pair<TEnum, value>> mapping
                                    )
    {
        return register_adapter(std::make_shared<enum_adapter_icase<TEnum>>(std::move(enum_name), mapping));
    }

    template <typename TPointer>
    polymorphic_adapter_builder<TPointer>
    polymorphic_type(std::string discrimination_key = "")
    {
        return polymorphic_adapter_builder<TPointer>(this, std::move(discrimination_key));
    }

    template <typename TPointer, typename F>
    polymorphic_adapter_builder<TPointer>
    polymorphic_type(std::string discrimination_key, F&& f)
    {
        return polymorphic_adapter_builder<TPointer>(this, std::move(discrimination_key), std::forward<F>(f));
    }

    template <typename F>
    formats_builder& extend(F&& func)
    {
        std::forward<F>(func)(*this);
        return *this;
    }

    formats_builder& register_adapter(const adapter* p)
    {
        _formats.register_adapter(p, _duplicate_type_action);
        return *this;
    }

    formats_builder& register_adapter(std::shared_ptr<const adapter> p)
    {
        _formats.register_adapter(std::move(p), _duplicate_type_action);
        return *this;
    }

    template <typename TOptional>
    formats_builder& register_optional()
    {
        reference_type(std::type_index(typeid(typename TOptional::value_type)), std::type_index(typeid(TOptional)));
        std::unique_ptr<optional_adapter<TOptional>> p(new optional_adapter<TOptional>);
        _formats.register_adapter(std::move(p), _duplicate_type_action);
        return *this;
    }

    template <typename TContainer>
    formats_builder& register_container()
    {
        reference_type(std::type_index(typeid(typename TContainer::value_type)), std::type_index(typeid(TContainer)));
        std::unique_ptr<container_adapter<TContainer>> p(new container_adapter<TContainer>);
        _formats.register_adapter(std::move(p), _duplicate_type_action);
        return *this;
    }

    template <typename TWrapper>
    formats_builder& register_wrapper()
    {
        reference_type(std::type_index(typeid(typename TWrapper::value_type)), std::type_index(typeid(TWrapper)));
        std::unique_ptr<wrapper_adapter<TWrapper>> p(new wrapper_adapter<TWrapper>);
        _formats.register_adapter(std::move(p), _duplicate_type_action);
        return *this;
    }

    template <typename T>
    formats_builder& register_containers()
    {
        return *this;
    }

    template <typename T, template <class...> class TTContainer, template <class...> class... TTRest>
    formats_builder& register_containers()
    {
        register_container<TTContainer<T>>();
        return register_containers<T, TTRest...>();
    }

    JSONV_NODISCARD
    operator formats() const
    {
        return _formats;
    }

    formats_builder& reference_type(std::type_index type);
    formats_builder& reference_type(std::type_index type, std::type_index from);

    /// \{
    /// Check that, when combined with the \c formats \a other, all types referenced by this \c formats_builder will
    /// get decoded properly.
    ///
    /// \param name if non-empty and this function throws, this \a name will be provided in the exception's \c what
    ///             string. This can be useful if you are running multiple \c check_references calls and you want to
    ///             name the different checks.
    ///
    /// \throws std::logic_error if \c formats this \c formats_builder is generating, when combined with the provided
    ///                          \a other \c formats, cannot properly serialize all the types.
    formats_builder& check_references(const formats&       other,  const std::string& name = "");
    formats_builder& check_references(const formats::list& others, const std::string& name = "");
    formats_builder& check_references(const std::string& name = "");
    /// \}

    /// \{
    /// Check the references of this builder (see \ref check_references) and compose a \ref formats instance if
    /// successful (see \ref formats::compose).
    JSONV_NODISCARD
    formats compose_checked(formats              other,  const std::string& name = "");
    JSONV_NODISCARD
    formats compose_checked(const formats::list& others, const std::string& name = "");
    /// \}

    /** Assigns the action to perform when a serializer or extractor is being registered by this formats_builder and
     *  there is already a serializer or extracter for that type.
    **/
    formats_builder& on_duplicate_type(duplicate_type_action action) noexcept;

private:
    void check_references_impl(const formats& searching, const std::string& name);

private:
    formats                                              _formats;
    duplicate_type_action                                _duplicate_type_action = duplicate_type_action::exception;
    std::map<std::type_index, std::set<std::type_index>> _referenced_types;
};

namespace detail
{

template <typename T>
adapter_builder<T> formats_builder_dsl::type()
{
    return owner->type<T>();
}

template <typename T, typename F>
adapter_builder<T> formats_builder_dsl::type(F&& f)
{
    return owner->type<T>(std::forward<F>(f));
}

template <typename TEnum>
formats_builder& formats_builder_dsl::enum_type(std::string                                    enum_name,
                                                std::initializer_list<std::pair<TEnum, value>> mapping
                                               )
{
    return owner->enum_type<TEnum>(std::move(enum_name), mapping);
}

template <typename TEnum>
formats_builder& formats_builder_dsl::enum_type_icase(std::string                                    enum_name,
                                                      std::initializer_list<std::pair<TEnum, value>> mapping
                                                     )
{
    return owner->enum_type_icase<TEnum>(std::move(enum_name), mapping);
}

template <typename TPointer>
polymorphic_adapter_builder<TPointer>
formats_builder_dsl::polymorphic_type(std::string discrimination_key)
{
    return owner->polymorphic_type<TPointer>(std::move(discrimination_key));
}

template <typename TPointer, typename F>
polymorphic_adapter_builder<TPointer>
formats_builder_dsl::polymorphic_type(std::string discrimination_key, F&& f)
{
    return owner->polymorphic_type<TPointer>(std::move(discrimination_key), std::forward<F>(f));
}

template <typename F>
formats_builder& formats_builder_dsl::extend(F&& f)
{
    return owner->extend(std::forward<F>(f));
}

template <typename TOptional>
formats_builder& formats_builder_dsl::register_optional()
{
    return owner->register_optional<TOptional>();
}

template <typename TContainer>
formats_builder& formats_builder_dsl::register_container()
{
    return owner->register_container<TContainer>();
}

template <typename T, template <class...> class... TTContainers>
formats_builder& formats_builder_dsl::register_containers()
{
    return owner->register_containers<T, TTContainers...>();
}

template <typename TWrapper>
formats_builder& formats_builder_dsl::register_wrapper()
{
    return owner->register_container<TWrapper>();
}

template <typename T>
adapter_builder<T>& adapter_builder_dsl<T>::type_default_on_null(bool on)
{
    return owner->type_default_on_null(on);
}

template <typename T>
adapter_builder<T>& adapter_builder_dsl<T>::type_default_value(std::function<T (extraction_context& ctx)> create)
{
    return owner->type_default_value(std::move(create));
}

template <typename T>
adapter_builder<T>& adapter_builder_dsl<T>::type_default_value(const T& value)
{
    return owner->type_default_value(value);
}

template <typename T>
template <typename TMember>
member_adapter_builder<T, TMember> adapter_builder_dsl<T>::member(std::string name, TMember T::*selector)
{
    return owner->member(std::move(name), selector);
}

template <typename T>
template <typename TMember>
member_adapter_builder<T, TMember>
adapter_builder_dsl<T>::member(std::string                              name,
                               std::function<const TMember& (const T&)> access,
                               std::function<void (T&, TMember&&)>      mutate
                              )
{
    return owner->member(std::move(name), std::move(access), std::move(mutate));
}

template <typename T>
template <typename TMember>
member_adapter_builder<T, TMember>
adapter_builder_dsl<T>::member(std::string    name,
                               const TMember& (T::*access)() const,
                               TMember&       (T::*mutable_access)()
                              )
{
    return owner->member(std::move(name), access, mutable_access);
}

template <typename T>
template <typename TMember>
member_adapter_builder<T, TMember>
adapter_builder_dsl<T>::member(std::string name,
                               const TMember& (T::*access)() const,
                               void (T::*mutate)(TMember)
                              )
{
    return owner->member(std::move(name), access, mutate);
}

template <typename T>
template <typename TMember>
member_adapter_builder<T, TMember>
adapter_builder_dsl<T>::member(std::string name,
                               const TMember& (T::*access)() const,
                               void (T::*mutate)(TMember&&)
                              )
{
    return owner->member(std::move(name), access, mutate);
}

template <typename T>
adapter_builder<T>& adapter_builder_dsl<T>::pre_extract(typename adapter_builder<T>::pre_extract_func perform)
{
    return owner->pre_extract(std::move(perform));
}

template <typename T>
adapter_builder<T>& adapter_builder_dsl<T>::post_extract(typename adapter_builder<T>::post_extract_func perform)
{
    return owner->post_extract(std::move(perform));
}

template <typename T>
adapter_builder<T>& adapter_builder_dsl<T>::on_extract_extra_keys(typename adapter_builder<T>::extra_keys_func handler)
{
    return owner->on_extract_extra_keys(std::move(handler));
}

}

/** Throw an \a extraction_error naming the \a extra_keys which claimed no member.
 *
 *  \throws extraction_error always.
**/
JSONV_NO_RETURN JSONV_PUBLIC
void throw_extra_keys_extraction_error(extraction_context&          context,
                                       const std::set<std::string>& extra_keys
                                      );

}

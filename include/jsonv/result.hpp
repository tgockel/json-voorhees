/// \file jsonv/result.hpp
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/detail/scope_exit.hpp>
#include <jsonv/optional.hpp>
#include <jsonv/string_view.hpp>

#include <exception>
#include <iosfwd>
#include <stdexcept>
#include <utility>
#include <variant>

namespace jsonv
{

template <typename TValue, typename TError>
class result;

template <typename TValue>
class ok;

template <typename TError>
class error;

}

namespace jsonv::detail
{

template <typename TResult1, typename TResult2, typename Enabler = void>
struct result_common_type
{ };

template <typename TValue1, typename TError1, typename TValue2, typename TError2>
struct result_common_type<jsonv::result<TValue1, TError1>,
                          jsonv::result<TValue2, TError2>,
                          std::void_t<std::common_type_t<TValue1, TValue2>, std::common_type_t<TError1, TError2>>
                         >
{
    using type = jsonv::result<std::common_type_t<TValue1, TValue2>, std::common_type_t<TError1, TError2>>;
};

}

namespace std
{

template <typename TValue1, typename TError1, typename TValue2, typename TError2>
struct common_type<jsonv::result<TValue1, TError1>, jsonv::result<TValue2, TError2>> :
        public jsonv::detail::result_common_type<jsonv::result<TValue1, TError1>, jsonv::result<TValue2, TError2>>
{ };

}

namespace jsonv::detail
{

template <template <typename...> class TT, typename Test>
struct is_template_of : public std::false_type
{ };

template <template <typename...> class TT, typename... TArgs>
struct is_template_of<TT, TT<TArgs...>> : public std::true_type
{ };

template <template <typename...> class TT, typename Test>
inline constexpr bool is_template_of_v = is_template_of<TT, Test>::value;

template <typename TResult, typename FUnary, typename Enabler = void>
struct result_map_result
{ };

template <typename TInputValue, typename TInputError, typename FUnary>
struct result_map_result<const result<TInputValue, TInputError>&,
                         FUnary,
                         std::void_t<std::invoke_result_t<FUnary, const TInputValue&>>
                        >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary, const TInputValue&>;

        using type = result<function_result_type, TInputError>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary, const TInputValue&>
                                         && std::is_nothrow_move_constructible_v<ok<function_result_type>>
                                         && std::is_nothrow_copy_constructible_v<TInputError>;
    };
};

template <typename TInputError, typename FUnary>
struct result_map_result<const result<void, TInputError>&, FUnary, std::void_t<std::invoke_result_t<FUnary>>>
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary>;

        using type = result<function_result_type, TInputError>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary>
                                         && std::is_nothrow_move_constructible_v<ok<function_result_type>>
                                         && std::is_nothrow_copy_constructible_v<TInputError>;
    };
};

template <typename TInputValue, typename TInputError, typename FUnary>
struct result_map_result<result<TInputValue, TInputError>&&,
                         FUnary,
                         std::void_t<std::invoke_result_t<FUnary, TInputValue&&>>
                        >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary, TInputValue&&>;

        using type = result<function_result_type, TInputError>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary, TInputValue&&>
                                         && std::is_nothrow_move_constructible_v<ok<function_result_type>>
                                         && std::is_nothrow_copy_constructible_v<TInputError>;
    };
};

template <typename TInputError, typename FUnary>
struct result_map_result<result<void, TInputError>&&, FUnary, std::void_t<std::invoke_result_t<FUnary>>>
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary>;

        using type = result<function_result_type, TInputError>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary>
                                         && std::is_nothrow_move_constructible_v<ok<function_result_type>>
                                         && std::is_nothrow_copy_constructible_v<TInputError>;
    };
};

template <typename TResult, typename FUnary>
using result_map_result_info = typename result_map_result<TResult, FUnary>::info;

template <typename TResult, typename FUnary, typename Enabler = void>
struct result_map_flat_result
{ };

template <typename TInputValue, typename TInputError, typename FUnary>
struct result_map_flat_result<const result<TInputValue, TInputError>&,
                              FUnary,
                              std::void_t<std::invoke_result_t<FUnary, const TInputValue&>>
                             >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary, const TInputValue&>;

        static_assert(is_template_of_v<result, function_result_type>,
                      "Function provided to `map_flat` must return a `jsonv::result`"
                     );

        using error_type = std::common_type_t<TInputError, typename function_result_type::error_type>;

        using type = result<typename function_result_type::value_type, error_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary, const TInputValue&>
                                         && std::is_nothrow_constructible_v<type, const error<TInputError>&>
                                         && std::is_nothrow_constructible_v<type, function_result_type&&>;
    };
};

template <typename TInputError, typename FUnary>
struct result_map_flat_result<const result<void, TInputError>&,
                              FUnary,
                              std::void_t<std::invoke_result_t<FUnary>>
                             >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary>;

        static_assert(is_template_of_v<result, function_result_type>,
                      "Function provided to `map_flat` must return a `jsonv::result`"
                     );

        using error_type = std::common_type_t<TInputError, typename function_result_type::error_type>;

        using type = result<typename function_result_type::value_type, error_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary>
                                         && std::is_nothrow_constructible_v<type, const error<TInputError>&>
                                         && std::is_nothrow_constructible_v<type, function_result_type&&>;
    };
};

template <typename TInputValue, typename TInputError, typename FUnary>
struct result_map_flat_result<result<TInputValue, TInputError>&&,
                              FUnary,
                              std::void_t<std::invoke_result_t<FUnary, TInputValue&&>>
                             >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary, TInputValue&&>;

        static_assert(is_template_of_v<result, function_result_type>,
                      "Function provided to `map_flat` must return a `jsonv::result`"
                     );

        using error_type = std::common_type_t<TInputError, typename function_result_type::error_type>;

        using type = result<typename function_result_type::value_type, error_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary, TInputValue&&>
                                         && std::is_nothrow_constructible_v<type, error<TInputError>&&>
                                         && std::is_nothrow_constructible_v<type, function_result_type&&>;
    };
};

template <typename TInputValue, typename TInputError, typename FUnary>
struct result_map_flat_result<result<TInputValue, TInputError>&&, FUnary, std::void_t<std::invoke_result_t<FUnary>>>
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary>;

        static_assert(is_template_of_v<result, function_result_type>,
                      "Function provided to `map_flat` must return a `jsonv::result`"
                     );

        using error_type = std::common_type_t<TInputError, typename function_result_type::error_type>;

        using type = result<typename function_result_type::value_type, error_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary>
                                         && std::is_nothrow_constructible_v<type, error<TInputError>&&>
                                         && std::is_nothrow_constructible_v<type, function_result_type&&>;
    };
};

template <typename TResult, typename FUnary>
using result_map_flat_result_info = typename result_map_flat_result<TResult, FUnary>::info;

template <typename TResult, typename FUnary, typename Enabler = void>
struct result_map_error_result
{ };

template <typename TInputValue, typename TInputError, typename FUnary>
struct result_map_error_result<const result<TInputValue, TInputError>&,
                               FUnary,
                               std::void_t<std::invoke_result_t<FUnary, const TInputError&>>
                              >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary, const TInputError&>;

        using type = result<TInputValue, function_result_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary, const TInputError&>
                                         && std::is_nothrow_move_constructible_v<error<function_result_type>>
                                         && std::is_nothrow_copy_constructible_v<TInputValue>;
    };
};

template <typename TInputValue, typename FUnary>
struct result_map_error_result<const result<TInputValue, void>&,
                               FUnary,
                               std::void_t<std::invoke_result_t<FUnary>>
                              >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary>;

        using type = result<TInputValue, function_result_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary>
                                         && std::is_nothrow_move_constructible_v<error<function_result_type>>
                                         && std::is_nothrow_copy_constructible_v<TInputValue>;
    };
};

template <typename TInputValue, typename TInputError, typename FUnary>
struct result_map_error_result<result<TInputValue, TInputError>&&,
                               FUnary,
                               std::void_t<std::invoke_result_t<FUnary, TInputError&&>>
                              >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary, TInputError&&>;

        using type = result<TInputValue, function_result_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary, TInputError&&>
                                         && std::is_nothrow_move_constructible_v<error<function_result_type>>
                                         && std::is_nothrow_move_constructible_v<TInputValue>;
    };
};

template <typename TInputValue, typename FUnary>
struct result_map_error_result<result<TInputValue, void>&&,
                               FUnary,
                               std::void_t<std::invoke_result_t<FUnary>>
                              >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary>;

        using type = result<TInputValue, function_result_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary>
                                         && std::is_nothrow_move_constructible_v<error<function_result_type>>
                                         && std::is_nothrow_move_constructible_v<TInputValue>;
    };
};

template <typename TResult, typename FUnary>
using result_map_error_result_info = typename result_map_error_result<TResult, FUnary>::info;

template <typename TResult, typename FUnary, typename Enabler = void>
struct result_recover_result
{ };

template <typename TInputValue, typename TInputError, typename FUnary>
struct result_recover_result<const result<TInputValue, TInputError>&,
                             FUnary,
                             std::void_t<std::invoke_result_t<FUnary, const TInputError&>>
                            >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary, const TInputError&>;

        static_assert(is_template_of_v<optional, function_result_type>,
                      "Function provided to `recover` must return a `jsonv::optional`"
                     );

        using function_result_value_type = typename function_result_type::value_type;

        using value_type = std::common_type_t<TInputValue, function_result_value_type>;

        using type = result<value_type, TInputError>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary, const TInputError&>
                                         && std::is_nothrow_constructible_v<value_type, function_result_value_type&&>
                                         && std::is_nothrow_constructible_v<value_type, const TInputValue&>
                                         && std::is_nothrow_copy_constructible_v<TInputError>;
    };
};

template <typename TInputValue, typename FUnary>
struct result_recover_result<const result<TInputValue, void>&,
                             FUnary,
                             std::void_t<std::invoke_result_t<FUnary>>
                            >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary>;

        static_assert(is_template_of_v<optional, function_result_type>,
                      "Function provided to `recover` must return a `jsonv::optional`"
                     );

        using function_result_value_type = typename function_result_type::value_type;

        using value_type = std::common_type_t<TInputValue, function_result_value_type>;

        using type = result<value_type, void>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary>
                                         && std::is_nothrow_constructible_v<value_type, function_result_value_type&&>
                                         && std::is_nothrow_constructible_v<value_type, const TInputValue&>;
    };
};

template <typename TInputError, typename FUnary>
struct result_recover_result<const result<void, TInputError>&,
                             FUnary,
                             std::void_t<std::invoke_result_t<FUnary, const TInputError&>>
                            >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary, const TInputError&>;

        static_assert(std::is_same_v<optional<ok<void>>, function_result_type>,
                      "Function provided to `recover` with `value_type` of `void` must return a "
                      "`jsonv::optional<jsonv::ok<void>>`"
                     );

        using type = result<void, TInputError>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary, const TInputError&>
                                         && std::is_nothrow_copy_constructible_v<TInputError>;
    };
};

template <typename FUnary>
struct result_recover_result<const result<void, void>&,
                             FUnary,
                             std::void_t<std::invoke_result_t<FUnary>>
                            >
{
    struct info
    {
        using function_result_type = std::invoke_result_t<FUnary>;

        static_assert(std::is_same_v<optional<ok<void>>, function_result_type>,
                      "Function provided to `recover` with `value_type` of `void` must return a "
                      "`jsonv::optional<jsonv::ok<void>>`"
                     );

        using type = result<void, void>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary>;
    };
};

template <typename TResult, typename FUnary>
using result_recover_result_info = typename result_recover_result<TResult, FUnary>::info;

template <typename TResult, typename FUnary, typename Enabler = void>
struct result_recover_flat_result
{ };

template <typename TInputValue, typename TInputError, typename FUnary>
struct result_recover_flat_result<const result<TInputValue, TInputError>&,
                                  FUnary,
                                  std::void_t<std::invoke_result_t<FUnary, const TInputError&>>
                                 >
{
    struct info
    {
        using source_type = result<TInputValue, TInputError>;

        using function_result_type = std::invoke_result_t<FUnary, const TInputError&>;

        static_assert(is_template_of_v<result, function_result_type>,
                      "Function provided to `recover_flat` must return a `jsonv::result`"
                     );

        using value_type = std::common_type_t<TInputValue, typename function_result_type::value_type>;

        using type = result<value_type, typename function_result_type::error_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary, const TInputError&>
                                         && std::is_nothrow_constructible_v<type, function_result_type&&>;
    };
};

template <typename TInputValue, typename FUnary>
struct result_recover_flat_result<const result<TInputValue, void>&,
                                  FUnary,
                                  std::void_t<std::invoke_result_t<FUnary>>
                                 >
{
    struct info
    {
        using source_type = result<TInputValue, void>;

        using function_result_type = std::invoke_result_t<FUnary>;

        static_assert(is_template_of_v<result, function_result_type>,
                      "Function provided to `recover_flat` must return a `jsonv::result`"
                     );

        using value_type = std::common_type_t<TInputValue, typename function_result_type::value_type>;

        using type = result<value_type, typename function_result_type::error_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary>
                                         && std::is_nothrow_constructible_v<type, function_result_type&&>;
    };
};

template <typename TInputValue, typename TInputError, typename FUnary>
struct result_recover_flat_result<result<TInputValue, TInputError>&&,
                                  FUnary,
                                  std::void_t<std::invoke_result_t<FUnary, TInputError&&>>
                                 >
{
    struct info
    {
        using source_type = result<TInputValue, TInputError>;

        using function_result_type = std::invoke_result_t<FUnary, TInputError&&>;

        static_assert(is_template_of_v<result, function_result_type>,
                      "Function provided to `recover_flat` must return a `jsonv::result`"
                     );

        using value_type = std::common_type_t<TInputValue, typename function_result_type::value_type>;

        using type = result<value_type, typename function_result_type::error_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary, TInputError&&>
                                         && std::is_nothrow_constructible_v<type, function_result_type&&>;
    };
};

template <typename TInputValue, typename FUnary>
struct result_recover_flat_result<result<TInputValue, void>&&,
                                  FUnary,
                                  std::void_t<std::invoke_result_t<FUnary>>
                                 >
{
    struct info
    {
        using source_type = result<TInputValue, void>;

        using function_result_type = std::invoke_result_t<FUnary>;

        static_assert(is_template_of_v<result, function_result_type>,
                      "Function provided to `recover_flat` must return a `jsonv::result`"
                     );

        using value_type = std::common_type_t<TInputValue, typename function_result_type::value_type>;

        using type = result<value_type, typename function_result_type::error_type>;

        static constexpr bool is_noexcept = std::is_nothrow_invocable_v<FUnary>
                                         && std::is_nothrow_constructible_v<type, function_result_type&&>;
    };
};

template <typename TResult, typename FUnary>
using result_recover_flat_result_info = typename result_recover_flat_result<TResult, FUnary>::info;

}

namespace jsonv
{

/// Describes the state of a \c result instance.
enum class result_state : unsigned char
{
    empty,
    ok,
    error,
};

JSONV_PUBLIC std::ostream& operator<<(std::ostream&, const result_state&);

/// A wrapper type for creating a \c result with \c result_state::ok. It is implicitly convertible to any \c result
/// with a convertible-to \c value_type.
///
/// \code
/// result<int, some_complicated_error_type> foo()
/// {
///     return ok(1);
/// }
/// \endcode
///
/// \see result
template <typename TValue>
class ok final
{
public:
    /// \see result::value_type
    using value_type = TValue;

public:
    /// Create an instance from the given \a args.
    template <typename... TArgs>
    explicit constexpr ok(TArgs&&... args) noexcept(std::is_nothrow_constructible_v<value_type, TArgs...>) :
            _value(std::forward<TArgs>(args)...)
    { }

    /// \{
    /// Get the value inside of this instance.
    const value_type& get() const& noexcept { return _value; }
    value_type&       get() &      noexcept { return _value; }
    value_type&&      get() &&     noexcept { return std::move(_value); }
    /// \}

private:
    value_type _value;
};

/// The \c void specialization of \c ok is meant for the creation of \c result instances with \c value_type of \c void.
///
/// \code
/// result<void> foo()
/// {
///     return ok{};
/// }
/// \endcode
///
/// \see result
template <>
class ok<void> final
{
public:
    explicit constexpr ok() noexcept = default;
};

template <typename TValue>
ok(TValue) -> ok<TValue>;

template <typename... TArgs>
ok(TArgs...) -> ok<std::enable_if_t<sizeof...(TArgs) == 0, void>>;

/// A wrapper type for creating a \c result with \c result_state::error. It is implicitly convertible to any \c result
/// with a convertible-to \c error_type.
///
/// \code
/// result<int, std::string> foo()
/// {
///     return error("foo went bad");
/// }
/// \endcode
template <typename TError>
class error final
{
public:
    /// \see result::error_type
    using error_type = TError;

public:
    template <typename... TArgs>
    explicit constexpr error(TArgs&&... args) noexcept(std::is_nothrow_constructible_v<error_type, TArgs...>) :
            _error(std::forward<TArgs>(args)...)
    { }

    const error_type& get() const& { return _error; }
    error_type&       get() &      { return _error; }
    error_type&&      get() &&     { return std::move(_error); }

private:
    error_type _error;
};

/// The \c void specialization of \c error is meant for the creation of \c result instances with \c error_type of
/// \c void.
///
/// \code
/// result<int, void> foo()
/// {
///     return error{};
/// }
/// \endcode
///
/// \see result
template <>
class error<void> final
{
public:
    explicit constexpr error() noexcept = default;
};

template <typename TError>
error(TError) -> error<TError>;

template <typename... TArgs>
error(TArgs...) -> error<std::enable_if_t<sizeof...(TArgs) == 0>>;

/// Thrown when an attempt to access the contents of a \c result was illegal. For example, calling \c value when the
/// \c result has \c result_state::error.
class JSONV_PUBLIC bad_result_access : public std::logic_error
{
public:
    /// Create an exception with the given \a description.
    explicit bad_result_access(const std::string& description) noexcept;

    /// Create an exception with a description noting the \a op_name must be performed in the \a expected state, but the
    /// \c result had a different \a actual state.
    static bad_result_access must_be(string_view op_name, result_state expected, result_state actual);

    /// Create an exception with a description noting the \a op_name must not be performed in the \a unexpected state.
    static bad_result_access must_not_be(string_view op_name, result_state unexpected);

    virtual ~bad_result_access() noexcept override;
};

/// The set of \c result operations which depend on the type of \c TValue. This is how \c result gets operations like
/// \c value_or, \c operator*, and \c operator->. If \c TValue is \c void, those operations are not available.
///
/// \see result
template <typename TSelf, typename TValue>
class result_value_operations
{
    using self_type = TSelf;

public:
    /// See \ref result::value_type.
    using value_type = TValue;

    /// \{
    /// Get the value inside of this result if it is \c result_state::ok.
    ///
    /// \throws bad_result_access if \c state is not \c result_state::ok.
    const value_type& value() const& { return get_raw("value"); }
    value_type&       value() &      { return get_raw("value"); }
    value_type&&      value() &&     { return std::move(get_raw("value")); }
    /// \}

    /// \{
    /// Get the value inside of this result if it is \c result_state::ok and returns \a recovery_value in other cases.
    template <typename UValue>
    constexpr value_type value_or(UValue&& recovery_value) const &
            noexcept(  std::is_nothrow_copy_constructible_v<value_type>
                    && std::is_nothrow_constructible_v<value_type, UValue>
                    )
    {
        auto& self = static_cast<self_type&>(*this);
        return self.is_ok() ? value() : static_cast<value_type>(std::forward<UValue>(recovery_value));
    }

    template <typename UValue>
    constexpr value_type value_or(UValue&& recovery_value) &&
            noexcept(  std::is_nothrow_move_constructible_v<value_type>
                    && std::is_nothrow_constructible_v<value_type, UValue>
                    )
    {
        auto& self = static_cast<self_type&>(*this);
        return self.is_ok() ? std::move(value()) : static_cast<value_type>(std::forward<UValue>(recovery_value));
    }
    /// \}

    /// \{
    /// Get the value inside of this result if it is \c result_state::ok.
    ///
    /// \throws bad_result_access if \c state is not \c result_state::ok.
    const value_type& operator*() const& { return get_raw("operator*"); }
    value_type&       operator*() &      { return get_raw("operator*"); }
    value_type&&      operator*() &&     { return std::move(get_raw("operator*")); }
    /// \}

    /// \{
    /// Get a pointer to the value inside of this result if it is \c result_state::ok.
    ///
    /// \throws bad_result_access if \c state is not \c result_state::ok.
    const value_type* operator->() const { return &get_raw("operator->"); }
    value_type*       operator->()       { return &get_raw("operator->"); }
    /// \}

private:
    inline constexpr const value_type& get_raw(const char* op_name) const
    {
        const auto& self = static_cast<const self_type&>(*this);
        self.ensure_state(op_name, result_state::ok);
        return std::get<self.ok_index>(self._storage).get();
    }

    inline constexpr value_type& get_raw(const char* op_name)
    {
        auto& self = static_cast<self_type&>(*this);
        self.ensure_state(op_name, result_state::ok);
        return std::get<self.ok_index>(self._storage).get();
    }
};

template <typename TSelf>
class result_value_operations<TSelf, void>
{
    using self_type = TSelf;

public:
    /// \throws bad_result_access if \c state is not \c result_state::ok.
    void value() const
    {
        const auto& self = static_cast<const self_type&>(*this);
        self.ensure_state("value", result_state::ok);
    }
};

/// The set of \c result operations which depend on the type of \c TError.
///
/// \see result
template <typename TSelf, typename TError>
class result_error_operations
{
    using self_type = TSelf;

public:
    /// See \ref result::error_type.
    using error_type = TError;

public:
    /// \{
    /// Get the error value inside of this result if it is \c result_state::error.
    ///
    /// \throws bad_result_access if \c state is not \c result_state::error.
    const error_type& error() const&
    {
        const auto& self = static_cast<const self_type&>(*this);
        self.ensure_state("error", result_state::error);
        return std::get<self.error_index>(self._storage).get();
    }

    error_type& error() &
    {
        auto& self = static_cast<self_type&>(*this);
        self.ensure_state("error", result_state::error);
        return std::get<self.error_index>(self._storage).get();
    }

    error_type&& error() &&
    {
        auto& self = static_cast<self_type&>(*this);
        self.ensure_state("error", result_state::error);
        return std::get<self.error_index>(std::move(self._storage)).get();
    }
    /// \}
};

template <typename TSelf>
class result_error_operations<TSelf, void>
{
    using self_type = TSelf;

public:
    /// \throws bad_result_access if \c state is not \c result_state::error.
    void error() const
    {
        const auto& self = static_cast<const self_type&>(*this);
        self.ensure_state("error", result_state::error);
    }
};

/// \{
/// Used to inform a \c result constructor that it should create a \c result_state::ok value in-place.
///
/// \code
/// result<std::mutex> mut(in_place_ok);
/// \endcode
///
/// \see result
/// \see ok
struct in_place_ok_t
{ };

static constexpr in_place_ok_t in_place_ok = in_place_ok_t();
/// \}

/// \{
/// Used to inform a \c result constructor that it should create a \c result_state::error value in-place.
///
/// \see result
/// \see error
struct in_place_error_t
{ };

static constexpr in_place_error_t in_place_error = in_place_error_t();
/// \}

/// \brief
/// A type that represents a value which might be a success (\c is_ok) or a failure (\c is_error). It is similar to
/// \c std::variant, but additional meaning is attached to the values, even if \c TValue and \c TError are the same
/// type. Returning a \c result should be seen as an alternative to exception-throwing or the use of \c errno. While it
/// is legal to throw from code which uses \c result, it is discouraged (see \ref result_error_handling_comparison for
/// more information).
///
/// Instances are transformed through member functions:
///
///  - \ref map : Transform an \c ok \c result into another \c ok with a different type.
///  - \ref map_flat : Transform an \c ok \c result into either an \c ok with a different type or an \c error with the
///    same type.
///  - \ref map_error : Transform an \c error \c result into another \c error with a different type.
///  - \ref recover : Attempt to recover an \c error \c result into an \c ok result with the same type.
///  - \ref recover_flat : Attempt to recover an \c error \c result into either an \c ok of the same type or an \c error
///    of a different type.
///
/// \dot
/// digraph result_states {
///   label = "Result State Transitions";
///
///   subgraph cluster_oks {
///     label = "";
///     style = "invis";
///
///     ok         [label="ok<TValue>",                       shape="rectangle", URL="\ref jsonv::ok"];
///     result_ok  [label="result<TValue, TError>\nstate=ok", shape="rectangle"];
///     value      [label="TValue", shape="rectangle"]
///   }
///
///   subgraph cluster_errors {
///     label = "";
///     style = "invis";
///
///     error      [label="error<TError>",                       shape="rectangle", URL="\ref jsonv::error"];
///     result_err [label="result<TValue, TError>\nstate=error", shape="rectangle"];
///     error_val  [label="TError", shape="rectangle"]
///   }
///
///   ok            -> result_ok    [label="ctor"]
///   error         -> result_err   [label="ctor"]
///
///   result_ok:sw  -> result_ok:nw   [label="map(F)"      , constraint="true",  URL="\ref jsonv::result::map"]
///   result_err:se -> result_err:ne  [label="map_error(F)", constraint="true",  URL="\ref jsonv::result::map_error"]
///   result_ok:ne  -> result_err:nw  [label="map_flat(F)" , constraint="false", URL="\ref jsonv::result::map_flat"]
///   result_err:sw -> result_ok:se   [label="recover(F)"  , constraint="false", URL="\ref jsonv::result::recover"]
///
///   result_ok     -> value        [label="value()", URL="\ref jsonv::result_value_operations::value"]
///   result_err    -> error_val    [label="error()", URL="\ref jsonv::result_error_operations::error"]
/// }
/// \enddot
///
/// A \c result instance is created with the \c ok and \c error helper types from some function performing an operation
/// which might fail.
///
/// \code
/// enum class math_error { domain, range };
///
/// result<double, math_error> sqrt_checked(double x) noexcept
/// {
///     if (x >= 0.0)
///         return ok{ std::sqrt(x) };
///     else
///         return error{ math_error::domain };
/// }
///
/// std::ostream& operator<<(std::ostream& os, const result<double, math_error>& r)
/// {
///     if (r.is_ok())
///     {
///         os << "ok(" << r.value() << ")";
///     }
///     else if (r.is_error())
///     {
///         os << "error(" << r.error() << ")";
///     }
///     else
///     {
///         // The default state of result is empty.
///         os << "empty";
///     }
/// }
///
/// int main()
/// {
///     std::cout << "sqrt(16) = " << sqrt_checked(16) << std::endl;
///     std::cout << "sqrt(-5) = " << sqrt_checked(-5) << std::endl;
/// }
/// \endcode
///
/// Program output:
///
/// \code{.sh}
/// sqrt(16) = ok(4)
/// sqrt(-5) = error(domain)
/// \endcode
///
/// The \c sqrt_checked function will give us \f$ \sqrt{x} \f$ when \f$ x \ge 0 \f$ and a \c math_error::domain error
/// when \f$ x < 0 \f$. What if we wanted to make a \c sqrt_div_2_checked function to compute \f$ \frac{\sqrt{x}}{2} \f$
/// using the result of \c sqrt_checked? While we could use \c is_ok to check if we should apply a `/2` operation, this
/// is easier to do with \ref map.
///
/// \code
/// result<double, math_error> sqrt_div_2_checked(double x) noexcept
/// {
///     return sqrt_checked(x)
///            .map([](double sqrt_x) { return sqrt_x / 2; });
/// }
///
/// int main()
/// {
///     std::cout << "sqrt(16) / 2 = " << sqrt_div_2_checked(16) << std::endl;
///     std::cout << "sqrt(-5) / 2 = " << sqrt_div_2_checked(-5) << std::endl;
/// }
/// \endcode
///
/// Program output:
///
/// \code{.sh}
/// sqrt(16) / 2 = ok(2)
/// sqrt(-5) / 2 = error(domain)
/// \endcode
///
/// The function provided to \ref map will not fail, as division by 2 works on all possible \c double values. But what
/// if we try to compute \f$ \arcsin\sqrt{x} \f$? The \c std::asin function requires input with range
/// \f$ \left[ -1, 1 \right] \f$, so let's write a checked version of it:
///
/// \code
/// result<double, math_error> asin_checked(double x) noexcept
/// {
///     if (x < -1.0 || x > 1.0)
///         return error{ math_error::domain };
///     else
///         return ok{ std::asin(x) };
/// }
///
/// result<double, math_error> asin_sqrt_checked(double x) noexcept
/// {
///     return sqrt_checked(x)
///            .map_flat(asin_checked);
/// }
///
/// int main()
/// {
///     std::cout << "asin(sqrt(16))  = " << asin_sqrt_checked(16)  << std::endl;
///     std::cout << "asin(sqrt(-5))  = " << asin_sqrt_checked(-5)  << std::endl;
///     std::cout << "asin(sqrt(0.5)) = " << asin_sqrt_checked(0.5) << std::endl;
/// }
/// \endcode
///
/// Program output:
///
/// \code{.sh}
/// asin(sqrt(16))  = error(domain)
/// asin(sqrt(-5))  = error(domain)
/// asin(sqrt(0.5)) = ok(0.785398)
/// \endcode
///
/// \section result_error_handling_comparison Comparison to Other Error Handling Techniques
///
/// Use of \c result for communication of operations which might fail is one of many error-handling techniques available
/// in C++. It is used in cases where there is no reasonable fallback value to return (in contrast to
/// \c jsonv::value::find).
///
/// Exceptions are a common mechanism to represent error cases, but are not always the ideal choice. While code relying
/// on exceptions is fast in the normal case, throwing an exception is much slower. As such, exception-throwing works
/// best in situations where errors are relatively rare. In cases where errors occur at a high or unknown rate (such as
/// parsing or extraction), techniques that do not pay a massive penalty to communicate errors are preferable.
///
/// One technique is returning a value (such as a \c bool or a dedicated error code) to indicate operation error. The
/// use of the C++ attribute `[[nodiscard]]` helps prevent the common issue of forgetting to check the result.
///
/// \code
/// [[nodiscard]]
/// error_type foo(const input_type& input, output_type& output);
/// \endcode
///
/// Unfortunately, this technique only works when \c output_type has a default constructor so calling code can create an
/// initial value for \c output. Furthermore, \c output_type must be mutable or assignable-to so implementing code can
/// populate \c output. There is also a question as to what should happen if \c output is passed as a non-empty value
/// (such as a \c std::vector which already has values). Should that be an error, should \c output be concatenated, or
/// should it be overwritten?
///
/// To address all these questions, we could have \c foo return either an \c optional or \c variant.
///
/// \code
/// std::variant<some_output_type, error_type> foo(const input_type& input);
/// \endcode
///
/// This solves a lot of the issues with the code-returning function. Aside from forcing callers to use the slightly
/// awkward \c std::get to access the result, this is close to ideal. Problems arise when either \c output_type or
/// \c error_type is \c void, as a \c variant can not store a \c void. In these cases, we could rely on \c optional to
/// represent the value or error, with \c nullopt indicating failure when \c error_type is \c void or success when
/// \c output_type is \c void. If they are both \c void, use \c bool.
///
///  1. `std::variant<output_type, error_type>` in cases where both \c output_type and \c error_type are not \c void
///  2. `std::optional<output_type>` in cases where \c output_type is not \c void, but \c error_type is
///  3. `std::optional<error_type>` in cases where \c output_type is \c void, but \c error_type is not
///  4. `bool` in case where both \c output_type and \c error_type are \c void
///
/// This all works fine if we are writing a non-generic \c foo, but what if the \c output_type or \c error_type of
/// \c foo is dependent on some other generic function? The type signatures for doing this can get a bit unwieldy:
///
/// \code
/// template <typename F>
/// std::conditional_t<std::is_void_v<std::invoke_result_t<F>>,
///                    std::optional<error_code>,
///                    std::variant<std::invoke_result_t<F>, error_code>
///                   >
/// foo(F&& func);
/// \endcode
///
/// Programmers now have a harder time figuring out what \c foo actually returns. Code relying this also needs to handle
/// this strange return type. Their usage is quite different to, as \c std::variant is accessed through the \c std::get
/// free function while \c std::optional has a member \c get and `operator bool` overload. The potential for accidental
/// misuse is quite high.
///
/// A \c result is meant to address all of the representational problems with error codes.
///
/// \code
/// template <typename F>
/// result<std::invoke_result_t<F>, error_code> foo(F&& func);
/// \endcode
///
/// The signature is quite a bit simpler.
///
/// JSON Voorhees uses a mixture of error-handling techniques depending on the situation and the nature of the error.
/// For cases where errors happen with unknown frequency, a \c result is used.
template <typename TValue, typename TError = std::exception_ptr>
class result final :
        public result_value_operations<result<TValue, TError>, TValue>,
        public result_error_operations<result<TValue, TError>, TError>
{
    using self_type = result<TValue, TError>;

    template <typename, typename>
    friend class result;

    template <typename, typename>
    friend class result_value_operations;

    template <typename, typename>
    friend class result_error_operations;

public:
    /// The type this result holds when it is \c result_state::ok. It is allowed to be \c void.
    using value_type = TValue;

    /// The type this result holds when it is \c result_state::error. It is allowed to be \c void.
    using error_type = TError;

public:
    /// Construct an empty result.
    constexpr result() noexcept :
            _storage()
    { }

    /// Construct a result with \c result_state::ok from the provided \a args.
    template <typename... TArgs>
    explicit constexpr result(in_place_ok_t, TArgs&&... args)
                noexcept(std::is_nothrow_constructible_v<value_type, TArgs...>) :
            _storage(std::in_place_index<ok_index>, std::forward<TArgs>(args)...)
    { }

    /// Construct a result with \c result_state::ok from the contents of \a value. This constructor is only enabled when
    /// \c TOkValue is an \ref ok with a \c value_type convertible to this result's \c value_type.
    template <typename TOkValue>
    constexpr result(TOkValue&& value,
                     std::enable_if_t<  detail::is_template_of_v<ok, std::decay_t<TOkValue>>
                                     && std::is_convertible_v<typename TOkValue::value_type, value_type>
                                     >* = nullptr
                    ) :
            result(in_place_ok, std::forward<TOkValue>(value).get())
    { }

    /// Construct a result with \c result_state::ok from a `ok<void>`. This constructor is only enabled when \c TOkValue
    /// is an `ok<void>` and this result has \c value_type of \c void.
    template <typename TOkValue>
    constexpr result(TOkValue&& value [[maybe_unused]],
                     std::enable_if_t<  std::is_same_v<ok<void>, std::decay_t<TOkValue>>
                                     && std::is_void_v<value_type>
                                     >* = nullptr
                    )
                noexcept:
            result(in_place_ok)
    { }

    /// Construct a result with \c result_state::ok from the provided \a value. This constructor is only enabled when
    /// \c UValue is convertible to \c value_type and \c value_type is distinct from \c error_type.
    template <typename UValue>
    constexpr result(UValue&& value,
                     std::enable_if_t< std::is_convertible_v<UValue, value_type>
                                     && !std::is_same_v<value_type, error_type>
                                     >* = nullptr
                    )
                noexcept(std::is_nothrow_constructible_v<value_type, UValue>) :
            result(in_place_ok, std::forward<UValue>(value))
    { }

    /// Construct a result with \c result_state::error from the provided \a args.
    template <typename... TArgs>
    explicit constexpr result(in_place_error_t, TArgs&&... args)
                noexcept(std::is_nothrow_constructible_v<error_type, TArgs...>) :
            _storage(std::in_place_index<error_index>, std::forward<TArgs>(args)...)
    { }

    /// Construct a result with \c result_state::error from the contents of \a error_value. This constructor is only
    /// enabled when \c TErrorValue is an \ref error with a \c error_type convertible to this result's \c error_type.
    template <typename TErrorValue>
    constexpr result(TErrorValue&& error_value,
                     std::enable_if_t<  detail::is_template_of_v<jsonv::error, TErrorValue>
                                     && std::is_convertible_v<typename TErrorValue::error_type, error_type>
                                     >* = nullptr
                    ) :
            result(in_place_error, std::forward<TErrorValue>(error_value).get())
    { }

    /// Construct a result with \c result_state::error from an `error<void>`. This constructor is only enabled when
    /// \c TErrorValue is an `error<void>` and this result has \c error_type of \c void.
    template <typename TErrorValue>
    constexpr result(TErrorValue&&,
                     std::enable_if_t<  std::is_same_v<jsonv::error<void>, std::decay_t<TErrorValue>>
                                     && std::is_void_v<error_type>
                                     >* = nullptr
                    )
                noexcept:
            result(in_place_error)
    { }

    /// Result instances are copy-constructible iff \c value_type and \c error_type are.
    constexpr result(const result&) = default;

    /// Result instances are move-constructible iff \c value_type and \c error_type are.
    constexpr result(result&&) noexcept = default;

    /// Result instances are copy-assignable iff \c value_type and \c error_type are.
    constexpr result& operator=(const result&) = default;

    /// Result instances are move-assignable iff \c value_type and \c error_type are.
    constexpr result& operator=(result&&) noexcept = default;

    /// Converting constructor from a \c result with a convertible \c value_type and \c error_type.
    template <typename UValue,
              typename UError,
              typename = std::enable_if_t<  std::is_convertible_v<UValue, value_type>
                                         && std::is_convertible_v<UError, error_type>
                                         >
             >
    result(const result<UValue, UError>& src)
    {
        switch (src.state())
        {
        case result_state::ok:
            if constexpr (std::is_void_v<value_type>)
            {
                _storage = ok{};
            }
            else
            {
                _storage = ok<value_type>(std::get<ok_index>(src._storage).get());
            }
            break;
        case result_state::error:
            if constexpr (std::is_void_v<error_type>)
            {
                _storage = jsonv::error{};
            }
            else
            {
                _storage = jsonv::error<error_type>(std::get<error_index>(src._storage).get());
            }
            break;
        case result_state::empty:
            // Do nothing -- we default-construct to empty
            break;
        }
    }

    /// Converting constructor from a \c result with a convertible \c value_type and \c error_type.
    template <typename UValue,
              typename UError,
              typename = std::enable_if_t<  std::is_convertible_v<UValue, value_type>
                                         && std::is_convertible_v<UError, error_type>
                                         >
             >
    result(result<UValue, UError>&& src) noexcept
    {
        auto clear_src = detail::on_scope_exit([&] { src.reset(); });
        switch (src.state())
        {
        case result_state::ok:
            if constexpr (std::is_void_v<value_type>)
            {
                _storage = ok{};
            }
            else
            {
                _storage = ok<value_type>(std::get<ok_index>(std::move(src._storage)).get());
            }
            break;
        case result_state::error:
            if constexpr (std::is_void_v<error_type>)
            {
                _storage = jsonv::error{};
            }
            else
            {
                _storage = jsonv::error<error_type>(std::get<error_index>(std::move(src._storage)).get());
            }
            break;
        case result_state::empty:
            // Do nothing -- we default-construct to empty
            break;
        }
    }

    /// Get the state of this object.
    constexpr result_state state() const noexcept
    {
        if (_storage.index() == ok_index)
            return result_state::ok;
        else if (_storage.index() == error_index)
            return result_state::error;
        else
            return result_state::empty;
    }

    /// Check that this result has \c state of \c result_state::ok.
    constexpr bool is_ok() const noexcept
    {
        return state() == result_state::ok;
    }

    /// Check that this result \c is_ok.
    constexpr explicit operator bool() const noexcept
    {
        return is_ok();
    }

    /// Check that this result has \c state of \c result_state::error.
    constexpr bool is_error() const noexcept
    {
        return state() == result_state::error;
    }

    /// Reset the contents of this instance to \c result_state::empty.
    void reset()
            noexcept(std::is_nothrow_destructible_v<value_type> && std::is_nothrow_destructible_v<error_type>)
    {
        _storage = std::monostate{};
    }

    /// \{
    /// Apply \a transform to a \c result_state::ok value, returning the result of the transformation in another result.
    /// If the instance is \c result_state::error, the error is copied into the returned value. If the instance is
    /// \c result_state::empty, an empty instance will be returned.
    ///
    /// This function is \c noexcept when calling \a transform is \c noexcept, the result is \c noexcept
    /// move-constructible, and the \c error_type is \c noexcept copy-constructible. It is highly encouraged to not
    /// throw from your \a transform function.
    ///
    /// \tparam FUnary `R (*)(const value_type&)` if \c value_type is not \c void or `R (*)()` if it is. The inferred
    ///                type \c R will be the \c value_type of the returned \c result (which can be \c void).
    ///
    /// \dot
    /// digraph result_map_xfrms {
    ///   rankdir = "LR";
    ///
    ///   subgraph cluster_init {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_1  [label="result<TValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_1 [label="result<TValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   subgraph cluster_fin {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_2  [label="result<UValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_2 [label="result<UValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   result_ok_1   -> result_ok_2  [label="transform(const TValue&) -> UValue"];
    ///   result_err_1  -> result_err_2 [label="transform not called"];
    /// }
    /// \enddot
    template <typename FUnary, typename ResultInfo = detail::result_map_result_info<const self_type&, FUnary>>
    typename ResultInfo::type map(FUnary&& transform) const& noexcept(ResultInfo::is_noexcept)
    {
        using return_type = typename ResultInfo::type;

        switch (state())
        {
        case result_state::ok:
            if constexpr (std::is_void_v<value_type>)
            {
                if constexpr (std::is_void_v<typename return_type::value_type>)
                {
                    std::forward<FUnary>(transform)();
                    return return_type(in_place_ok);
                }
                else
                {
                    return return_type(in_place_ok, std::forward<FUnary>(transform)());
                }
            }
            else
            {
                if constexpr (std::is_void_v<typename return_type::value_type>)
                {
                    std::forward<FUnary>(transform)(std::get<ok_index>(_storage).get());
                    return return_type(in_place_ok);
                }
                else
                {
                    return return_type(in_place_ok,
                                       std::forward<FUnary>(transform)(std::get<ok_index>(_storage).get())
                                      );
                }
            }
        case result_state::error:
            return return_type(in_place_error, std::get<error_index>(_storage));
        case result_state::empty:
        default:
            return return_type();
        }
    }

    /// Apply \a transform to a \c result_state::ok value, returning the result of the transformation in another result.
    /// If the instance is \c result_state::error, the error is moved into the returned value. If the instance is
    /// \c result_state::empty, an empty instance will be returned.
    ///
    /// This function is \c noexcept when calling \a transform is \c noexcept, the result is \c noexcept
    /// move-constructible, and the \c error_type is \c noexcept move-constructible. It is highly encouraged to not
    /// throw from your \a transform function.
    ///
    /// \tparam FUnary `R (*)(value_type&&)` if \c value_type is not \c void or `R (*)()` if it is. The inferred type
    ///                \c R will be the \c value_type of the returned \c result (which can be \c void).
    ///
    /// \dot
    /// digraph result_map_xfrms {
    ///   rankdir = "LR";
    ///
    ///   subgraph cluster_init {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_1  [label="result<TValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_1 [label="result<TValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   subgraph cluster_fin {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_2  [label="result<UValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_2 [label="result<UValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   result_ok_1   -> result_ok_2  [label="transform(TValue&&) -> UValue"];
    ///   result_err_1  -> result_err_2 [label="transform not called"];
    /// }
    /// \enddot
    template <typename FUnary, typename ResultInfo = detail::result_map_result_info<self_type&&, FUnary>>
    typename ResultInfo::type map(FUnary&& transform) && noexcept(ResultInfo::is_noexcept)
    {
        using return_type = typename ResultInfo::type;

        auto clear_me = detail::on_scope_exit([&] { reset(); });

        switch (state())
        {
        case result_state::ok:
            if constexpr (std::is_void_v<value_type>)
            {
                if constexpr (std::is_void_v<typename return_type::value_type>)
                {
                    std::forward<FUnary>(transform)();
                    return return_type(in_place_ok);
                }
                else
                {
                    return return_type(in_place_ok, std::forward<FUnary>(transform)());
                }
            }
            else
            {
                if constexpr (std::is_void_v<typename return_type::value_type>)
                {
                    std::forward<FUnary>(transform)(std::get<ok_index>(std::move(_storage)).get());
                    return return_type(in_place_ok);
                }
                else
                {
                    return return_type(in_place_ok,
                                       std::forward<FUnary>(transform)(std::get<ok_index>(std::move(_storage)).get())
                                      );
                }
            }
        case result_state::error:
            return return_type(in_place_error, std::get<error_index>(std::move(_storage)));
        case result_state::empty:
        default:
            return return_type();
        }
    }
    /// \}

    /// \{
    /// Apply \a transform to a \c result_state::ok value, returning the result of the transformation, which itself is
    /// a \c result. If the instance is \c result_state::error, the error is copied into the returned value. If the
    /// instance is \c result_state::empty, an empty instance will be returned.
    ///
    /// This function is \c noexcept when calling \a transform is \c noexcept, the result is \c noexcept
    /// move-constructible, and the \c error_type of the returned type is \c noexcept constructible from this instance's
    /// \c error_type. It is highly encouraged to not throw from your \a transform function.
    ///
    /// \tparam FUnary `result<R, E> (*)(const value_type&)` if \c value_type is not \c void or `result<R, E> (*)()` if
    ///                it is. Type \c E in the result must have common type with \c error_type.
    ///
    /// \dot
    /// digraph result_map_flat_xfrms {
    ///   rankdir = "LR";
    ///
    ///   subgraph cluster_init {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_1  [label="result<TValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_1 [label="result<TValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   subgraph cluster_fin {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_2  [label="result<UValue, UError>\nstate=ok",    shape="rectangle"];
    ///     result_err_2 [label="result<UValue, UError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   result_ok_1   -> result_ok_2  [label="transform(const TValue&) returns ok(UValue)"];
    ///   result_ok_1   -> result_err_2 [label="transform(const TValue&) returns error(UError)"];
    ///   result_err_1  -> result_err_2 [label="transform not called"];
    /// }
    /// \enddot
    template <typename FUnary, typename ResultInfo = detail::result_map_flat_result_info<const self_type&, FUnary>>
    typename ResultInfo::type
    map_flat(FUnary&& transform) const& noexcept(ResultInfo::is_noexcept)
    {
        using return_type = typename ResultInfo::type;

        switch (state())
        {
        case result_state::ok:
            if constexpr (std::is_void_v<value_type>)
            {
                return std::forward<FUnary>(transform)();
            }
            else
            {
                return std::forward<FUnary>(transform)(std::get<ok_index>(_storage).get());
            }
        case result_state::error:
            return return_type(in_place_error, std::get<error_index>(_storage));
        case result_state::empty:
        default:
            return return_type();
        }
    }

    /// Apply \a transform to a \c result_state::ok value, returning the result of the transformation, which itself is
    /// a \c result. If the instance is \c result_state::error, the error is moved into the returned value. If the
    /// instance is \c result_state::empty, an empty instance will be returned.
    ///
    /// This function is \c noexcept when calling \a transform is \c noexcept, the result is \c noexcept
    /// move-constructible, and the \c error_type of the returned type is \c noexcept constructible from this instance's
    /// \c error_type. It is highly encouraged to not throw from your \a transform function.
    ///
    /// \tparam FUnary `result<R, E> (*)(value_type&&)` if \c value_type is not \c void or `result<R, E> (*)()` if it
    ///                is. Type \c E in the result must have common type with \c error_type.
    ///
    /// \dot
    /// digraph result_map_flat_xfrms {
    ///   rankdir = "LR";
    ///
    ///   subgraph cluster_init {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_1  [label="result<TValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_1 [label="result<TValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   subgraph cluster_fin {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_2  [label="result<UValue, UError>\nstate=ok",    shape="rectangle"];
    ///     result_err_2 [label="result<UValue, UError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   result_ok_1   -> result_ok_2  [label="transform(TValue&&) returns ok(UValue)"];
    ///   result_ok_1   -> result_err_2 [label="transform(TValue&&) returns error(UError)"];
    ///   result_err_1  -> result_err_2 [label="transform not called"];
    /// }
    /// \enddot
    template <typename FUnary, typename ResultInfo = detail::result_map_flat_result_info<self_type&&, FUnary>>
    typename ResultInfo::type
    map_flat(FUnary&& transform) && noexcept(ResultInfo::is_noexcept)
    {
        using return_type = typename ResultInfo::type;

        auto clear_me = detail::on_scope_exit([&] { reset(); });
        switch (state())
        {
        case result_state::ok:
            if constexpr (std::is_void_v<value_type>)
            {
                return std::forward<FUnary>(transform)();
            }
            else
            {
                return std::forward<FUnary>(transform)(std::get<ok_index>(std::move(_storage)).get());
            }
        case result_state::error:
            return return_type(in_place_error, std::get<error_index>(std::move(_storage)));
        case result_state::empty:
        default:
            return return_type();
        }
    }
    /// \}

    /// \{
    /// Apply \a transform to a \c result_state::error value, returning the result of the transformation in another
    /// result. If the instance is \c result_state::ok, the value is copied into the returned value. If the instance is
    /// \c result_state::empty, an empty instance will be returned.
    ///
    /// This function is \c noexcept when calling \a transform is \c noexcept, the result is \c noexcept
    /// move-constructible, and the \c value_type is \c noexcept copy-constructible. It is highly encouraged to not
    /// throw from your \a transform function.
    ///
    /// \tparam FUnary `E (*)(const error_type&)` if \c error_type is not \c void or `E (*)()` if it is. The inferred
    ///                type \c E will be the \c error_type of the returned \c result (which can be \c void).
    ///
    /// \dot
    /// digraph result_map_error_xfrms {
    ///   rankdir = "LR";
    ///
    ///   subgraph cluster_init {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_1  [label="result<TValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_1 [label="result<TValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   subgraph cluster_fin {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_2  [label="result<TValue, UError>\nstate=ok",    shape="rectangle"];
    ///     result_err_2 [label="result<TValue, UError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   result_ok_1   -> result_ok_2  [label="transform not called"];
    ///   result_err_1  -> result_err_2 [label="transform(const TError&) -> UError"];
    /// }
    /// \enddot
    template <typename FUnary, typename ResultInfo = detail::result_map_error_result_info<const self_type&, FUnary>>
    typename ResultInfo::type
    map_error(FUnary&& transform) const& noexcept(ResultInfo::is_noexcept)
    {
        using return_type = typename ResultInfo::type;

        switch (state())
        {
        case result_state::ok:
            return return_type(in_place_ok, std::get<ok_index>(_storage));
        case result_state::error:
            if constexpr (std::is_void_v<error_type>)
            {
                if constexpr (std::is_void_v<typename return_type::error_type>)
                {
                    std::forward<FUnary>(transform)();
                    return return_type(in_place_error);
                }
                else
                {
                    return return_type(in_place_error, std::forward<FUnary>(transform)());
                }
            }
            else
            {
                if constexpr (std::is_void_v<typename return_type::error_type>)
                {
                    std::forward<FUnary>(transform)(std::get<error_index>(_storage).get());
                    return return_type(in_place_error);
                }
                else
                {
                    return return_type(in_place_error,
                                       std::forward<FUnary>(transform)(std::get<error_index>(_storage).get())
                                      );
                }
            }
        case result_state::empty:
        default:
            return return_type();
        }
    }

    /// Apply \a transform to a \c result_state::error value, returning the result of the transformation in another
    /// result. If the instance is \c result_state::ok, the value is moved into the returned value. If the instance is
    /// \c result_state::empty, an empty instance will be returned.
    ///
    /// This function is \c noexcept when calling \a transform is \c noexcept, the result is \c noexcept
    /// move-constructible, and the \c value_type is \c noexcept move-constructible. It is highly encouraged to not
    /// throw from your \a transform function.
    ///
    /// \tparam FUnary `E (*)(error_type&&)` if \c error_type is not \c void or `E (*)()` if it is. The inferred type
    ///                \c E will be the \c error_type of the returned \c result (which can be \c void).
    ///
    /// \dot
    /// digraph result_map_error_xfrms {
    ///   rankdir = "LR";
    ///
    ///   subgraph cluster_init {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_1  [label="result<TValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_1 [label="result<TValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   subgraph cluster_fin {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_2  [label="result<TValue, UError>\nstate=ok",    shape="rectangle"];
    ///     result_err_2 [label="result<TValue, UError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   result_ok_1   -> result_ok_2  [label="transform not called"];
    ///   result_err_1  -> result_err_2 [label="transform(TError&&) -> UError"];
    /// }
    /// \enddot
    template <typename FUnary, typename ResultInfo = detail::result_map_error_result_info<self_type&&, FUnary>>
    typename ResultInfo::type
    map_error(FUnary&& transform) && noexcept(ResultInfo::is_noexcept)
    {
        using return_type = typename ResultInfo::type;

        auto clear_me = detail::on_scope_exit([&] { reset(); });

        switch (state())
        {
        case result_state::ok:
            return return_type(in_place_ok, std::get<ok_index>(std::move(_storage)));
        case result_state::error:
            if constexpr (std::is_void_v<error_type>)
            {
                if constexpr (std::is_void_v<typename return_type::error_type>)
                {
                    std::forward<FUnary>(transform)();
                    return return_type(in_place_error);
                }
                else
                {
                    return return_type(in_place_error, std::forward<FUnary>(transform)());
                }
            }
            else
            {
                if constexpr (std::is_void_v<typename return_type::error_type>)
                {
                    std::forward<FUnary>(transform)(std::get<error_index>(std::move(_storage)).get());
                    return return_type(in_place_error);
                }
                else
                {
                    return return_type(in_place_error,
                                       std::forward<FUnary>(transform)(std::get<error_index>(std::move(_storage)).get())
                                      );
                }
            }
        case result_state::empty:
        default:
            return return_type();
        }
    }
    /// \}

    /// \{
    /// Apply \a recovery to a \c result_state::error value, returning the result of the transformation if it returns an
    /// \c optional instance which has a value. If \a recovery returns \c nullopt, this instance's \c error is copied to
    /// the returned value. If this instance is \c result_state::ok, \a recovery is not called and the current value is
    /// copied to the result. If the instance is \c result_state::empty, an empty instance will be returned.
    ///
    /// This function is \c noexcept when calling \a transform is \c noexcept, the \c error_type is \c noexcept copy
    /// constructible, and the common type of \c value_type and the \a recovery function's \c value_type is \c noexcept
    /// copy constructible from \c value_type and \c noexcept move constructible from the \a recovery function's
    /// \c value_type. It is highly recommended to not throw from your \a recovery function.
    ///
    /// \param recovery The function to call in an attempt to recover from an error. This function returns a valued
    ///                 \c optional instance to indicate the error has been recovered from with the value to recover to.
    ///                 Returning a \c nullopt indicates the error was not recovered from.
    /// \tparam FUnary `optional<R> (*)(const error_type&)` if \c error_type is not \c void or `optional<R> (*)()` if it
    ///                is. The type \c R in the result must have common type with \c value_type if \c value_type is not
    ///                \c void and must be \c ok<void> if it is.
    ///
    /// \dot
    /// digraph result_recover_xfrms {
    ///   rankdir = "LR";
    ///
    ///   subgraph cluster_init {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_1  [label="result<TValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_1 [label="result<TValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   subgraph cluster_fin {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_2  [label="result<UValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_2 [label="result<UValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   result_ok_1   -> result_ok_2  [label="recovery not called"];
    ///   result_err_1  -> result_ok_2  [label="recovery(const TError&) -> some(UValue)"];
    ///   result_err_1  -> result_err_2 [label="recovery(const TError&) -> nullopt"];
    /// }
    /// \enddot
    ///
    /// \note
    /// There is no \c && qualified overload of \c recover. In \c result_state::error cases, the \c error_type instance
    /// would be moved into the \a recovery function, which should be passed back in cases where the error could not be
    /// recovered from. This would require the signature for \c FUnary to be something which could return either a
    /// value or an error, which is covered by \c recover_flat.
    template <typename FUnary, typename ResultInfo = detail::result_recover_result_info<const self_type&, FUnary>>
    typename ResultInfo::type
    recover(FUnary&& recovery) const& noexcept(ResultInfo::is_noexcept)
    {
        using return_type = typename ResultInfo::type;

        switch (state())
        {
        case result_state::ok:
            if constexpr (std::is_void_v<value_type>)
            {
                return return_type(in_place_ok);
            }
            else
            {
                return return_type(in_place_ok, std::get<ok_index>(_storage).get());
            }
        case result_state::error:
            {
                using optional_type = typename ResultInfo::function_result_type;

                auto intermediate =
                    [&]() -> optional_type
                    {
                        if constexpr (std::is_void_v<error_type>)
                        {
                            return std::forward<FUnary>(recovery)();
                        }
                        else
                        {
                            return std::forward<FUnary>(recovery)(std::get<error_index>(_storage).get());
                        }
                    }();

                if (intermediate)
                {
                    return return_type(in_place_ok, std::move(*intermediate));
                }
                else
                {
                    return return_type(in_place_error, std::get<error_index>(_storage));
                }
            }
        case result_state::empty:
        default:
            return return_type();
        }
    }
    /// \}

    /// \{
    /// Apply \a recovery to a \c result_state::error value, returning the result of the transformation, which itself is
    /// a \c result. If the instance is \c result_state::ok, the value is copied into the returned value. If the
    /// instance is \c result_state::empty, an empty instance will be returned.
    ///
    /// This function is \c noexcept when calling \a recovery is \c noexcept, the result is \c noexcept
    /// move-constructible, and the \c value_type of the returned type is \c noexcept constructible from this instance's
    /// \c value_type. It is highly encouraged to not throw from your \a recovery function.
    ///
    /// \tparam FUnary `result<R, E> (*)(const error_type&)` if \c error_type is not \c void or `result<R, E> (*)()` if
    ///                it is. Type \c R in the result must have common type with \c value_type.
    ///
    /// \dot
    /// digraph result_recover_flat_xfrms {
    ///   rankdir = "LR";
    ///
    ///   subgraph cluster_init {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_1  [label="result<TValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_1 [label="result<TValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   subgraph cluster_fin {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_2  [label="result<UValue, UError>\nstate=ok",    shape="rectangle"];
    ///     result_err_2 [label="result<UValue, UError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   result_ok_1   -> result_ok_2  [label="recovery not called"];
    ///   result_err_1  -> result_ok_2  [label="recovery(const TError&) -> UValue"];
    ///   result_err_1  -> result_err_2 [label="recovery(const TError&) -> UError"];
    /// }
    /// \enddot
    template <typename FUnary, typename ResultInfo = detail::result_recover_flat_result_info<const self_type&, FUnary>>
    typename ResultInfo::type
    recover_flat(FUnary&& recovery) const& noexcept(ResultInfo::is_noexcept)
    {
        using return_type = typename ResultInfo::type;

        switch (state())
        {
        case result_state::ok:
            if constexpr (std::is_void_v<value_type>)
            {
                return return_type(in_place_ok);
            }
            else
            {
                return return_type(in_place_ok, std::get<ok_index>(_storage).get());
            }
        case result_state::error:
            if constexpr (std::is_void_v<error_type>)
            {
                return std::forward<FUnary>(recovery)();
            }
            else
            {
                return std::forward<FUnary>(recovery)(std::get<error_index>(_storage).get());
            }
        case result_state::empty:
        default:
            return return_type();
        }
    }

    /// Apply \a recovery to a \c result_state::error value, returning the result of the transformation, which itself is
    /// a \c result. If the instance is \c result_state::ok, the value is copied into the returned value. If the
    /// instance is \c result_state::empty, an empty instance will be returned.
    ///
    /// This function is \c noexcept when calling \a recovery is \c noexcept, the result is \c noexcept
    /// move-constructible, and the \c value_type of the returned type is \c noexcept constructible from this instance's
    /// \c value_type. It is highly encouraged to not throw from your \a recovery function.
    ///
    /// \tparam FUnary `result<R, E> (*)(error_type&&)` if \c error_type is not \c void or `result<R, E> (*)()` if it
    ///                is. Type \c R in the result must have common type with \c value_type.
    ///
    /// \dot
    /// digraph result_recover_flat_xfrms {
    ///   rankdir = "LR";
    ///
    ///   subgraph cluster_init {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_1  [label="result<TValue, TError>\nstate=ok",    shape="rectangle"];
    ///     result_err_1 [label="result<TValue, TError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   subgraph cluster_fin {
    ///     label = "";
    ///     style = "invis";
    ///
    ///     result_ok_2  [label="result<UValue, UError>\nstate=ok",    shape="rectangle"];
    ///     result_err_2 [label="result<UValue, UError>\nstate=error", shape="rectangle"];
    ///   }
    ///
    ///   result_ok_1   -> result_ok_2  [label="recovery not called"];
    ///   result_err_1  -> result_ok_2  [label="recovery(TError&&) -> UValue"];
    ///   result_err_1  -> result_err_2 [label="recovery(TError&&) -> UError"];
    /// }
    /// \enddot
    template <typename FUnary, typename ResultInfo = detail::result_recover_flat_result_info<self_type&&, FUnary>>
    typename ResultInfo::type
    recover_flat(FUnary&& recovery) && noexcept(ResultInfo::is_noexcept)
    {
        using return_type = typename ResultInfo::type;

        auto clear_me = detail::on_scope_exit([&] { reset(); });
        switch (state())
        {
        case result_state::ok:
            if constexpr (std::is_void_v<value_type>)
            {
                return return_type(in_place_ok);
            }
            else
            {
                return return_type(in_place_ok, std::get<ok_index>(std::move(_storage)).get());
            }
        case result_state::error:
            if constexpr (std::is_void_v<error_type>)
            {
                return std::forward<FUnary>(recovery)();
            }
            else
            {
                return std::forward<FUnary>(recovery)(std::get<error_index>(std::move(_storage)).get());
            }
        case result_state::empty:
        default:
            return return_type();
        }
    }
    /// \}

private:
    using storage_type = std::variant<std::monostate, ok<value_type>, jsonv::error<error_type>>;

    static constexpr std::size_t empty_index = 0U;
    static constexpr std::size_t ok_index    = 1U;
    static constexpr std::size_t error_index = 2U;

    constexpr void ensure_state(const char* op_name, result_state expected) const
    {
        if (state() != expected)
            throw bad_result_access::must_be(op_name, expected, state());
    }

private:
    storage_type _storage;
};

template <typename T>
result(ok<T>) -> result<T>;

}

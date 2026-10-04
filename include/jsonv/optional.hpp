/// \file jsonv/optional.hpp
/// Pulls in an implementation of \c optional.
///
/// Copyright (c) 2016-2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace jsonv
{

namespace detail
{

/// Does binding a \c T& to a \c U, as direct-initialization does, refer to an object which is already there -- an
/// lvalue, or the one something converts to, as a \c std::reference_wrapper does -- rather than to a temporary made for
/// the binding?
///
/// This is <tt>std::is_constructible_v<T&, U> && !std::reference_constructs_from_temporary_v<T&, U></tt>, for the \c U
/// a forwarding reference deduces, which is what \c std::optional<T&> asks. That trait is C++23 and not every standard
/// library has it, so where it is missing the answer is worked out from what a reference can bind to:
///
///  - A reference to non-\c const binds to nothing a temporary could be made for, so whatever it binds to is there.
///  - An lvalue of \c T, or of a class derived from it, is bound to directly.
///  - Any other class reaches a \c T& by converting. A conversion which makes a new object makes something a
///    <tt>T&&</tt> binds to as well, and one which yields an lvalue, explicitly or not, does not.
///
/// That never takes a binding the trait would refuse, but it refuses a few the trait would take: a conversion yielding
/// an xvalue, and -- since GCC lets a class's own copy constructor make a <tt>T&&</tt> out of what a conversion yields
/// -- a \c std::reference_wrapper to a class with a user-declared copy constructor. Neither arises where the trait is
/// there, which is every toolchain this library is built with in CI except, possibly, MSVC's and Apple's.
template <typename T, typename U>
concept binds_without_temporary =
       std::is_constructible_v<T&, U>
#if defined(__cpp_lib_reference_from_temporary) && __cpp_lib_reference_from_temporary >= 202202L
    && !std::reference_constructs_from_temporary_v<T&, U>;
#else
    && (  !std::is_const_v<T>
       || (std::is_lvalue_reference_v<U> && std::is_convertible_v<std::remove_reference_t<U>*, T*>)
       || (  std::is_class_v<std::remove_cvref_t<U>>
          && !std::is_convertible_v<std::remove_reference_t<U>*, T*>
          && !std::is_constructible_v<T&&, U>
          )
       );
#endif

/// What \c jsonv::optional is for a reference: \c std::optional<T&> as C++26 specifies it (P2988, with P3836), for a
/// library which cannot require C++26 yet. \a T is the type referred to, and may be \c const.
///
/// It holds a pointer and nothing else -- an empty one is a null pointer -- so it costs what the pointer it stands in
/// for would, and it is trivially copyable. Assigning to one rebinds it rather than assigning through to the object it
/// refers to. It binds to anything a \c T& binds to without making a temporary, a \c std::reference_wrapper among
/// them, and refuses whatever would leave it referring to a temporary, which would be gone by the end of the
/// statement. All of that is what \c std::optional<T&> does.
///
/// It has the constructors, observers and modifiers of \c std::optional<T&>, and compares with \c std::nullopt. It
/// does not have the iterator or monadic members, which nothing here uses. Add them to the standard's specification
/// before reaching for them, so that replacing this with \c std::optional changes nothing.
template <typename T>
class optional_ref final
{
public:
    using value_type = T;

public:
    /// An empty instance.
    constexpr optional_ref() noexcept = default;

    /// An empty instance.
    constexpr optional_ref(std::nullopt_t) noexcept
    { }

    /// Refer to what \a ref is, or converts to -- implicitly, where a \c U converts to a \c T& implicitly.
    template <typename U>
        requires (!std::is_same_v<std::remove_cvref_t<U>, optional_ref> && binds_without_temporary<T, U>)
    constexpr explicit(!std::is_convertible_v<U, T&>) optional_ref(U&& ref)
            noexcept(std::is_nothrow_constructible_v<T&, U>) :
            _ptr(std::addressof(static_cast<T&>(std::forward<U>(ref))))
    { }

    /// Binding to a temporary would leave this referring to an object which no longer exists.
    template <typename U>
        requires (  !std::is_same_v<std::remove_cvref_t<U>, optional_ref>
                 && !binds_without_temporary<T, U>
                 && std::is_constructible_v<T&, U>
                 )
    optional_ref(U&&) = delete;

    /// Refer to whatever \a other refers to, where a \c U& converts to a \c T& -- as a \c value& does to a
    /// <tt>const value&</tt>.
    template <typename U>
        requires (!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
    constexpr optional_ref(const optional_ref<U>& other) noexcept :
            _ptr(other._ptr)
    { }

    /// Make this empty.
    constexpr optional_ref& operator=(std::nullopt_t) noexcept
    {
        _ptr = nullptr;
        return *this;
    }

    /// Does this refer to anything?
    JSONV_NODISCARD
    constexpr bool has_value() const noexcept
    {
        return _ptr != nullptr;
    }

    /// See \ref has_value.
    JSONV_NODISCARD
    constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    /// \{

    /// Get the object referred to. Like \c std::optional, this does not check: on an empty instance the behavior is
    /// undefined. \ref value is the checked form.
    JSONV_NODISCARD
    constexpr T& operator*() const noexcept
    {
        return *_ptr;
    }

    JSONV_NODISCARD
    constexpr T* operator->() const noexcept
    {
        return _ptr;
    }
    /// \}

    /// Get the object referred to.
    ///
    /// \throws std::bad_optional_access if this is empty.
    JSONV_NODISCARD
    constexpr T& value() const
    {
        if (!_ptr)
            throw std::bad_optional_access();

        return *_ptr;
    }

    /// Get a copy of the object referred to, or \a fallback if this is empty. A copy, because \a fallback is
    /// usually a temporary and a reference to it would dangle.
    template <typename U = std::remove_cv_t<T>>
    JSONV_NODISCARD
    constexpr std::remove_cv_t<T> value_or(U&& fallback) const
    {
        if (_ptr)
            return *_ptr;
        else
            return static_cast<std::remove_cv_t<T>>(std::forward<U>(fallback));
    }

    /// Refer to what \a ref is, or converts to, instead of whatever this referred to before, and return it.
    template <typename U>
        requires binds_without_temporary<T, U>
    constexpr T& emplace(U&& ref) noexcept(std::is_nothrow_constructible_v<T&, U>)
    {
        T& target = static_cast<T&>(std::forward<U>(ref));
        _ptr = std::addressof(target);
        return target;
    }

    /// Binding to a temporary would leave this referring to an object which no longer exists.
    template <typename U>
        requires (!binds_without_temporary<T, U> && std::is_constructible_v<T&, U>)
    T& emplace(U&&) = delete;

    /// Make this empty.
    constexpr void reset() noexcept
    {
        _ptr = nullptr;
    }

    constexpr void swap(optional_ref& other) noexcept
    {
        std::swap(_ptr, other._ptr);
    }

    friend constexpr void swap(optional_ref& a, optional_ref& b) noexcept
    {
        a.swap(b);
    }

    JSONV_NODISCARD
    friend constexpr bool operator==(const optional_ref& self, std::nullopt_t) noexcept
    {
        return !self._ptr;
    }

private:
    template <typename U>
    friend class optional_ref;

private:
    T* _ptr = nullptr;
};

template <typename T>
struct optional_type
{
    using type = std::optional<T>;
};

template <typename T>
struct optional_type<T&>
{
    using type = optional_ref<T>;
};

}

/// Represents a value that may or may not be present.
///
/// This is \c std::optional<T> for every \a T it accepts. A reference is the exception: \c std::optional of one is
/// ill-formed until C++26 (P2988), and the standard libraries of MSVC and Apple's Clang do not have it yet. Until every
/// toolchain this library supports does, <tt>optional<T&></tt> is \c detail::optional_ref, which behaves as
/// \c std::optional<T&> does as far as it goes. Once they all do, this is \c std::optional throughout.
///
/// It is chosen by the reference rather than by which standard library is present, so that the library and a program
/// built against it in another language mode agree on what every signature means.
template <typename T>
using optional = typename detail::optional_type<T>::type;

}

/// \file jsonv/detail/basic_view.hpp
///
/// Copyright (c) 2014-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

#include <iterator>
#include <utility>

namespace jsonv::detail
{

/// A view template used for array and object views of a \c value. This class allows traversing a \c value with a range
/// based for loop.
///
/// \note
/// A view does nothing to preserve the lifetime of the underlying container, nor does it remain valid if the container
/// is modified.
template <typename TIterator,
          typename TConstIterator = TIterator
         >
class basic_view
{
public:
    typedef TIterator                                           iterator;
    typedef TConstIterator                                      const_iterator;
    typedef std::reverse_iterator<iterator>                     reverse_iterator;
    typedef std::reverse_iterator<const_iterator>               const_reverse_iterator;
    typedef typename std::iterator_traits<iterator>::value_type value_type;
    typedef typename std::iterator_traits<iterator>::reference  reference;
    typedef typename std::iterator_traits<iterator>::pointer    pointer;

public:
    basic_view(iterator begin_, iterator end_) :
            _begin(begin_),
            _end(end_)
    { }

    JSONV_NODISCARD
    iterator       begin()       { return _begin; }
    JSONV_NODISCARD
    const_iterator begin() const { return _begin; }
    JSONV_NODISCARD
    iterator       end()         { return _end; }
    JSONV_NODISCARD
    const_iterator end() const   { return _end; }

    JSONV_NODISCARD
    const_iterator cbegin() const { return _begin; }
    JSONV_NODISCARD
    const_iterator cend()   const { return _end; }

    JSONV_NODISCARD
    reverse_iterator       rbegin()       { return reverse_iterator(end()); };
    JSONV_NODISCARD
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    JSONV_NODISCARD
    reverse_iterator       rend()         { return reverse_iterator(begin()); }
    JSONV_NODISCARD
    const_reverse_iterator rend() const   { return const_reverse_iterator(begin()); }

    JSONV_NODISCARD
    const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
    JSONV_NODISCARD
    const_reverse_iterator crend() const   { return const_reverse_iterator(begin()); }

private:
    iterator _begin;
    iterator _end;
};

/// Something that owns an object. Used in basic_owning_view to move the value somewhere fixed before getting the
/// itertors and constructing the base.
template <typename T>
class basic_owner
{
public:
    explicit basic_owner(T&& x) :
            _value(std::move(x))
    { }

protected:
    T _value;
};

/// A form of basic_view that owns the object it is iterating over.
///
/// The iterators can refer to the container's address (an array iterator holds a pointer to its \c value), so they
/// cannot simply be carried along when the view moves. A moved-to view rebuilds them from the container it now owns,
/// which is why it keeps the functions it was constructed with. For the same reason, an owning view can be moved but
/// not copied: a copy would own a fresh container while its iterators still pointed into the source's.
///
/// A moved-from view is an empty range. It is safe to destroy, assign to, move from again, or iterate (zero times).
template <typename TContainer,
          typename TIterator,
          typename TConstIterator = TIterator
         >
class basic_owning_view :
        private basic_owner<TContainer>,
        public basic_view<TIterator, TConstIterator>
{
    using basic_owner<TContainer>::_value;
    using view_type = basic_view<TIterator, TConstIterator>;

public:
    /// A function which finds one end of a container. It must not hold state, so an owning view can keep it.
    using locate_function = TIterator (*)(TContainer&);

    basic_owning_view(TContainer&& container, locate_function locate_begin, locate_function locate_end) :
            basic_owner<TContainer>(std::move(container)),
            view_type(locate_begin(_value), locate_end(_value)),
            _locate_begin(locate_begin),
            _locate_end(locate_end)
    { }

    basic_owning_view(const basic_owning_view&)            = delete;
    basic_owning_view& operator=(const basic_owning_view&) = delete;

    basic_owning_view(basic_owning_view&& src) noexcept :
            basic_owner<TContainer>(std::move(src._value)),
            view_type(TIterator(), TIterator()),
            _locate_begin(nullptr),
            _locate_end(nullptr)
    {
        adopt_view_of(src);
    }

    basic_owning_view& operator=(basic_owning_view&& src) noexcept
    {
        if (this != &src)
        {
            _value = std::move(src._value);
            adopt_view_of(src);
        }

        return *this;
    }

private:
    /// Point this view at the container it now owns, and leave \a src as an empty range. The container has already
    /// been moved. If \a src was itself moved from, its container is empty and has no ends to find, so this view is
    /// empty too.
    void adopt_view_of(basic_owning_view& src) noexcept
    {
        _locate_begin = std::exchange(src._locate_begin, nullptr);
        _locate_end   = std::exchange(src._locate_end,   nullptr);

        view_type::operator=(_locate_begin ? view_type(_locate_begin(_value), _locate_end(_value))
                                           : view_type(TIterator(), TIterator())
                            );
        src.view_type::operator=(view_type(TIterator(), TIterator()));
    }

private:
    locate_function _locate_begin;
    locate_function _locate_end;
};

}

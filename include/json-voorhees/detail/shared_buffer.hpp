/** \file json-voorhees/detail/shared_buffer.hpp
 *  
 *  Copyright (c) 2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSON_VOORHEES_DETAIL_SHARED_BUFFER_HPP_INCLUDED__
#define __JSON_VOORHEES_DETAIL_SHARED_BUFFER_HPP_INCLUDED__

#include <json-voorhees/config.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace jsonv
{
namespace detail
{

/** \brief
 *  
 *  A \c shared_buffer has lazy copy-on-write semantics.  When you assign a buffer from another or take a slice from
 *  another, a reference is added, but no data is copied and new memory is not reserved.  When writing data, if this
 *  buffer is not unique, the data will be copied to a new location in memory before writing occurs.  If you think this
 *  could be dangerous, check \c is_unique before performing write operations.
**/
class JSONV_PUBLIC shared_buffer
{
public:
    typedef std::size_t    size_type;
    typedef std::ptrdiff_t difference_type;
    typedef char           value_type;
    typedef char&          reference;
    typedef const char&    const_reference;
    typedef char*          pointer;
    typedef const char*    const_pointer;
    
public:
    /** \brief Not a position. **/
    static constexpr size_type npos = -45UL;
    
public:
    /** \brief Creates a new, 0 length shared buffer. **/
    shared_buffer();
    
    /** \brief Creates a new shared buffer with the specified size.
     *  
     *  \warning
     *  The data contained in the new buffer will be random garbage.  If you want it to be zero-filled, use
     *  \c create_zero_filled.
     *  
     *  \param size The size of the new buffer in bytes.
    **/
    explicit shared_buffer(size_type size);
    
    /** \brief Create an instance from given \a src and \a size. **/
    shared_buffer(const_pointer src, size_type size);
    
    /** \brief Copies this buffer from another.
     *
     *  This is actually quite a cheap operation -- since the backing data is the same, no copying is needed to create
     *  a local version (unless \a copy_now is \c true).
     *  
     *  \param other The buffer to take the backing data from.
     *  \param copy_now If \c true, then the buffer will create a unique copy of the data after creation.  This can be
     *   helpful when you do not want to get hit with memory operations later.
    **/
    shared_buffer(const shared_buffer& other, bool copy_now = false);
    
    /** \brief Assigns this shared buffer. **/
    shared_buffer& operator=(const shared_buffer& other);
    
    /** \brief Move construction is faster than copying. **/
    shared_buffer(shared_buffer&& other) throw();
    shared_buffer& operator=(shared_buffer&& other) throw();
    
    /** \brief Create a buffer with the given \a size filled with zeros. **/
    static shared_buffer create_zero_filled(size_type size);
    
    ~shared_buffer() throw();
    
    const_pointer cbegin() const
    {
        if (_data)
            return _data.get();
        else
            return nullptr;
    }
    
    const_pointer cend() const
    {
        if (_data)
            return _data.get() + _length;
        else
            return nullptr;
    }
    
    /** \brief Is the buffer uniquely pointing at the underlying data?
     *  
     *  If this is \c true, write operations on this buffer will not allocate memory.  This will also return \c true if
     *  this is an empty buffer, since 
    **/
    inline bool is_unique() const
    {
        return !_data || _data.unique();
    }
    
    /** \brief Gets the size of this buffer in bytes.
     *  
     *  If this is a slice, the backing buffer might be much larger than the value here, as a slice is restricted to a
     *  subset of a whole.
    **/
    inline size_type size() const
    {
        return _length;
    }
    
    /** \brief Get an immutable view of the buffer at a particular position.
     *  
     *  \see get_mutable
    **/
    const char* get(size_type index = 0UL, size_type read_size = npos) const
    {
        if (read_size != npos)
            ensure_index(index, read_size);
        
        return _data.get() + index;
    }
    
    /** \brief Get an mutable pointer to the buffer at a particular position.
     *  If this shared_buffer is not unique, it will be made unique.
     *  
     *  \see get
    **/
    char* get_mutable(size_type index = 0UL, size_type read_size = npos)
    {
        if (read_size != npos)
            ensure_index(index, read_size);
        
        make_unique();
        return _data.get() + index;
    }
    
    /** \brief Copies the data contained in the backing buffer to a new location in memory.
     *
     *  \returns \c true if data was actually copied; \c false if this buffer was already unique.
    **/
    bool make_unique();
    
    /** \brief Creates a slice of this shared buffer for use other places.
     *  
     *  The result of a slice is equivalent to D and Python: [0, 1, 2, 3].slice(1, 2) == [1, 2].
     *  
     *  \param start_idx The beginning index to start the slice at.  If this is npos, assume the start of this buffer.
     *  \param end_idx The final index to end the slice at.  If this is npos, assume the end of this buffer.
    **/
    shared_buffer slice(size_type start_idx = npos, size_type end_idx = npos) const;
    
    /** \brief Slices until the given index. **/
    inline shared_buffer slice_until(size_type end_idx) const
    {
        return slice(npos, end_idx);
    }
    
    /** \brief Slices from the given index to the end of the buffer. **/
    inline shared_buffer slice_to_end(size_type start_idx) const
    {
        return slice(start_idx, npos);
    }
    
    /** \brief Swaps the contents of this buffer with other. **/
    void swap(shared_buffer& other) throw();
    
    /** \brief Quickly checks for equality with other.
     *  
     *  This simply checks that the underlying buffer, the offset and length of \c this and \a other are the same.
     *  
     *  \returns \c true if the two buffers are the same; \c false if the two buffers are not backed by the same set.
     *   Keep in mind that this does not mean that the \e contents of the buffers are not equal.
    **/
    inline bool operator ==(const shared_buffer& other) const
    {
        return _data == other._data
            && _length == other._length;
    }
    
    /** \brief Quickly checks for inequality with other.
     *
     *  \returns \c true if the two buffers are not the same; \c false if the two buffers are backed by the same set.
     *   Like \c operator==, this does not guarentee that the \c contents of the two buffers are not equal.  However,
     *   if this returns \c false, then they must be equal, since they point to the same region of memory.
    **/
    inline bool operator !=(const shared_buffer& other) const
    {
        return _data != other._data
            || _length != other._length;
    }
    
    /** \brief Checks the contents of two shared buffers for equality.
     *
     *  In the worst case, this takes O(\c size) time, since the \c shared_buffer could end up having to check the
     *  complete contents of memory.
    **/
    bool contents_equal(const shared_buffer& other) const;
    
private:
    inline void ensure_index(size_type index, size_type read_size) const
    {
        if (index + read_size > _length)
            throw std::range_error("index + read_size out of range");
    }
    
private:
    std::shared_ptr<char> _data;
    size_type             _length;
};

inline void swap(shared_buffer& a, shared_buffer& b) throw()
{
    a.swap(b);
}

}
}

#endif/*__JSON_VOORHEES_DETAIL_SHARED_BUFFER_HPP_INCLUDED__*/

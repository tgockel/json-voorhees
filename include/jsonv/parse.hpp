/** \file jsonv/parse.hpp
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_PARSE_HPP_INCLUDED__
#define __JSONV_PARSE_HPP_INCLUDED__

#include "standard.hpp"
#include "value.hpp"

#include <cstddef>
#include <deque>
#include <stdexcept>

namespace jsonv
{

/** An error encountered when parsing.
 *  
 *  \see parse
**/
class JSONV_PUBLIC parse_error :
        public std::runtime_error
{
public:
    typedef std::size_t size_type;
    
    /** Description of a single parsing problem. **/
    struct problem
    {
    public:
        problem(size_type line, size_type column, size_type character, std::string message);
        
        /** The line of input this error was encountered on. A new "line" is determined by carriage return or line feed.
         *  If you are in Windows and line breaks are two characters, the line number of the error will appear to be
         *  twice as high as you would think.
        **/
        size_type line() const
        {
            return _line;
        }
        
        /** The character index on the current line this error was encountered on. **/
        size_type column() const
        {
            return _column;
        }
        
        /** The character index into the entire input this error was encountered on. **/
        size_type character() const
        {
            return _character;
        }
        
        /** A message from the parser which has user-readable details about the encountered problem. **/
        const std::string& message() const
        {
            return _message;
        }
        
    private:
        size_type   _line;
        size_type   _column;
        size_type   _character;
        std::string _message;
    };
    
    typedef std::deque<problem> problem_list;
    
public:
    parse_error(problem_list, value partial_result);
    
    virtual ~parse_error() throw();
    
    /** The list of problems which ultimately contributed to this \c parse_error. There will always be at least one
     *  \c problem in this list.
    **/
    const problem_list& problems() const;
    
    /** Get the partial result of parsing. There is no guarantee this value even resembles the input JSON as the input
     *  JSON was malformed.
    **/
    const value& partial_result() const;
    
private:
    problem_list _problems;
    value        _partial_result;
};

/** Configuration for various parsing options. **/
class JSONV_PUBLIC parse_options
{
public:
    /** When a parse error is encountered, what should the parser do? **/
    enum class on_error
    {
        /** Immediately throw a \c parse_error -- do not attempt to construct a partial result. **/
        fail_immediately,
        /** Attempt to continue parsing and constructing a result. **/
        collect_all,
        /** Ignore all errors and pretend to be successful. This is not recommended unless you are 100% certain the
         *  JSON you are attempting to parse is valid. Using this failure mode does not improve parser performance.
        **/
        ignore,
    };
    
public:
    /** Create an instance with the default options. **/
    parse_options();
    
    /** See \c on_error. The default failure mode is \c fail_immediately. **/
    on_error failure_mode() const;
    parse_options& failure_mode(on_error mode);
    
    /** The maximum allowed parsing failures the parser can encounter before throwing an error. This is only applicable
     *  if the \c failure_mode is not \c on_error::fail_immediately. By default, this value is 10.
     *  
     *  You should probably not set this value to an unreasonably high number, as each parse error encountered must be
     *  stored in memory for some period of time.
    **/
    std::size_t max_failures() const;
    parse_options& max_failures(std::size_t limit);
    
private:
    // For the purposes of ABI compliance, most modifications to the variables in this class should bump the minor
    // version number.
    on_error    _failure_mode;
    std::size_t _max_failures;
};

/** Construct a JSON value from the given input.
 *  
 *  \note
 *  This function is \e not intended for verifying if the input is valid JSON, as it will intentionally correctly parse
 *  invalid JSON. This library ignores extra commas in objects and arrays and will add commas between key-value pairs in
 *  objects and elements in arrays.
 *  
 *  \throws parse_error if an error is found in the JSON. If the \a input terminates unexpectedly, a \c parse_error will
 *   still be thrown with a message like "Unexpected end: unmatched {...". If you suspect the input of going bad, you
 *   can check the state flags or set the exception mask of the stream (exceptions thrown by \a input while processing
 *   will be propagated out).
**/
value JSONV_PUBLIC parse(const char* input, std::size_t length, const parse_options& = parse_options());

/** Reads a JSON value from the input stream.
 *  
 *  \see parse(const char*, std::size_t length)
**/
value JSONV_PUBLIC parse(std::istream& input, const parse_options& = parse_options());

/** Reads a JSON value from a string.
 *  
 *  \see parse(const char*, std::size_t length)
**/
value JSONV_PUBLIC parse(const std::string& input, const parse_options& = parse_options());

}

#endif/*__JSONV_PARSE_HPP_INCLUDED__*/

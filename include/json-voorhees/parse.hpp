#ifndef __JSON_VOORHEES_PARSE_HPP_INCLUDED__
#define __JSON_VOORHEES_PARSE_HPP_INCLUDED__

#include "standard.hpp"
#include "value.hpp"

#include <stdexcept>

namespace jsonv
{

/** An error encountered when parsing.
 *  
 *  \see parse
**/
class parse_error :
        public std::runtime_error
{
public:
    parse_error(unsigned line, unsigned column, unsigned character, const std::string& message);
    
    virtual ~parse_error() throw();
    
    /** The line of input this error was encountered on. A new "line" is determined by carriage return or line feed. If
     *  you are in Windows and line breaks are two characters, the line number of the error will appear to be twice as
     *  high as you would think.
    **/
    unsigned line() const
    {
        return _line;
    }
    
    /// The character index on the current line this error was encountered on.
    unsigned column() const
    {
        return _column;
    }
    
    /// The character index into the entire input this error was encountered on.
    unsigned character() const
    {
        return _character;
    }
    
    /// A message from the parser which has user-readable details about the encountered problem.
    const std::string& message() const
    {
        return _message;
    }
    
private:
    unsigned    _line;
    unsigned    _column;
    unsigned    _character;
    std::string _message;
};

/** Reads a JSON value from the input stream.
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
value parse(istream_type& input);

/** Reads a JSON value from a string.
 *  
 *  \see parse(istream_type&)
**/
value parse(const string_type& input);

}

#endif/*__JSON_VOORHEES_PARSE_HPP_INCLUDED__*/

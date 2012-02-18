#ifndef __JSON_VOORHEES_CHAR_CONVERT_HPP_INCLUDED__
#define __JSON_VOORHEES_CHAR_CONVERT_HPP_INCLUDED__

#include <json-voorhees/standard.hpp>

#include <stdexcept>

namespace jsonv
{
namespace detail
{

class decode_error :
        public std::runtime_error
{
public:
    typedef string_type::size_type size_type;
public:
    decode_error(size_type offset, const std::string& message);
    
    virtual ~decode_error() throw();
    
    inline size_type offset() const
    {
        return _offset;
    }
    
private:
    size_type _offset;
};

/** Encodes C++ string \a source into a fully-escaped JSON string into \a stream ready for sending over the wire.
**/
ostream_type& string_encode(ostream_type& stream, const string_type& source);

/** Decodes over the wire string \c source into a C++ string.
**/
string_type string_decode(const string_type& source);

}
}

#endif/*__JSON_VOORHEES_CHAR_CONVERT_HPP_INCLUDED__*/

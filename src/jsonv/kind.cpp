#include <jsonv/kind.hpp>

#include <sstream>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// kind                                                                                                               //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const kind& k)
{
    switch (k)
    {
        case jsonv::kind::array:   return os << "array";
        case jsonv::kind::boolean: return os << "boolean";
        case jsonv::kind::decimal: return os << "decimal";
        case jsonv::kind::integer: return os << "integer";
        case jsonv::kind::null:    return os << "null";
        case jsonv::kind::object:  return os << "object";
        case jsonv::kind::string:  return os << "string";
        default:                   return os << "kind(" << +static_cast<unsigned char>(k) << ")";
    }
}

std::string to_string(const kind& k)
{
    std::ostringstream ss;
    ss << k;
    return ss.str();
}

}

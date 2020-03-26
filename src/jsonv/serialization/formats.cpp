#include <jsonv/serialization/formats.hpp>
#include <jsonv/demangle.hpp>

#include <sstream>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// duplicate_type_error                                                                                               //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string make_duplicate_type_errmsg(string_view operation, const std::type_index& type)
{
    std::ostringstream os;
    os << "Already have " << operation << " for type " << demangle(type.name());
    return os.str();
}

duplicate_type_error::duplicate_type_error(string_view operation, const std::type_index& type) :
        std::invalid_argument(make_duplicate_type_errmsg(operation, type)),
        _type_index(type)
{ }

duplicate_type_error::~duplicate_type_error() noexcept = default;

}

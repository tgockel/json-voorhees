#include <jsonv/all.hpp>

#include <iostream>
#include <string_view>

int main()
{
    constexpr std::string_view prerelease = JSONV_VERSION_PRERELEASE;

    jsonv::value value = jsonv::parse(R"({"answer": 42})");
    std::cout << value
              << " (jsonv " << JSONV_VERSION_MAJOR << '.' << JSONV_VERSION_MINOR << '.' << JSONV_VERSION_PATCH
              << (prerelease.empty() ? "" : "-") << prerelease
              << ", JSONV_SO=" << JSONV_SO << ")\n";
    return value.at("answer").as_integer() == 42 ? 0 : 1;
}

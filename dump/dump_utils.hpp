#pragma once

#include <map>
#include <string>
#include <variant>
namespace openpower
{
namespace dump
{
namespace util
{

using DumpCreateParams =
    std::map<std::string, std::variant<std::string, uint64_t>>;

} // namespace util
} // namespace dump
} // namespace openpower

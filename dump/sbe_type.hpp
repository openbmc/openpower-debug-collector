#pragma once

#include <map>
#include <string>

namespace openpower
{
namespace dump
{

enum class SBETypes
{
    PROC,
};

struct SBEAttributes
{
    std::string chipName;
    std::string chipOpFailure;
};

extern std::map<SBETypes, SBEAttributes> sbeTypeAttributes;
} // namespace dump
} // namespace openpower

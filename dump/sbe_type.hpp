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
    OCMB
};

struct SBEAttributes
{
    std::string chipName;
    std::string dumpPath;
    std::string chipOpTimeout;
    std::string chipOpFailure;
};

extern std::map<SBETypes, SBEAttributes> sbeTypeAttributes;
} // namespace dump
} // namespace openpower

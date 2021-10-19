#pragma once

#include <map>
#include <string>

namespace openpower::dump
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

extern const std::map<SBETypes, SBEAttributes> sbeTypeAttributes;
} // namespace openpower::dump

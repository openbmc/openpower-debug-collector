#pragma once

#include <map>
#include <string>

namespace openpower::dump
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
    std::string noFfdc;
    std::string sbeInternalFFDCData;
};

extern const std::map<SBETypes, SBEAttributes> sbeTypeAttributes;
} // namespace openpower::dump

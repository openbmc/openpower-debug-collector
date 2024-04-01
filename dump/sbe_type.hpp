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
<<<<<<< HEAD
=======
    OCMB
>>>>>>> d940ae8 (Support for HW dump from Odyssey)
};

struct SBEAttributes
{
    std::string chipName;
<<<<<<< HEAD
=======
    std::string dumpPath;
    std::string timeoutError;
>>>>>>> d940ae8 (Support for HW dump from Odyssey)
    std::string chipOpFailure;
};

extern std::map<SBETypes, SBEAttributes> sbeTypeAttributes;
} // namespace dump
} // namespace openpower

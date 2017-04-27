// This file was autogenerated.  Do not edit!
// See elog-gen.py for more details
#pragma once

#include <string>
#include <tuple>
#include <type_traits>
#include <sdbusplus/exception.hpp>
#include <phosphor-logging/log.hpp>

namespace sdbusplus
{
namespace org
{
namespace open_power
{
namespace Host
{
namespace Error
{
    struct Checkstop;
} // namespace Error
} // namespace Host
} // namespace open_power
} // namespace org
} // namespace sdbusplus


namespace phosphor
{

namespace logging
{

namespace org
{
namespace open_power
{
namespace Host
{
namespace _Checkstop
{


}  // namespace _Checkstop

struct Checkstop : public sdbusplus::exception_t
{
    static constexpr auto errName = "org.open_power.Host.Checkstop";
    static constexpr auto errDesc = "Checkstop condition detected";
    static constexpr auto L = level::ERR;
    using metadata_types = std::tuple<>;

    const char* name() const noexcept
    {
        return errName;
    }

    const char* description() const noexcept
    {
        return errDesc;
    }

    const char* what() const noexcept
    {
        return errName;
    }
};

} // namespace Host
} // namespace open_power
} // namespace org


namespace details
{

template <>
struct map_exception_type<sdbusplus::org::open_power::Host::Error::Checkstop>
{
    using type = org::open_power::Host::Checkstop;
};

}


} // namespace logging

} // namespace phosphor

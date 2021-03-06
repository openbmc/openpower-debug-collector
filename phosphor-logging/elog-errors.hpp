// This file was autogenerated.  Do not edit!
// See elog-gen.py for more details
#pragma once

#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/exception.hpp>

#include <string>
#include <tuple>
#include <type_traits>

namespace sdbusplus
{
namespace org
{
namespace open_power
{
namespace Host
{
namespace Boot
{
namespace Error
{
struct WatchdogTimedOut;
} // namespace Error
} // namespace Boot
} // namespace Host
} // namespace open_power
} // namespace org
} // namespace sdbusplus

namespace sdbusplus
{
namespace org
{
namespace open_power
{
namespace Host
{
namespace Boot
{
namespace Error
{
struct Checkstop;
} // namespace Error
} // namespace Boot
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
namespace Boot
{
namespace _Checkstop
{} // namespace _Checkstop

struct Checkstop
{
    static constexpr auto L = level::ERR;
    using metadata_types = std::tuple<>;
};

} // namespace Boot
} // namespace Host
} // namespace open_power
} // namespace org

namespace details
{

template <>
struct map_exception_type<
    sdbusplus::org::open_power::Host::Boot::Error::Checkstop>
{
    using type = org::open_power::Host::Boot::Checkstop;
};

} // namespace details

namespace org
{
namespace open_power
{
namespace Host
{
namespace Boot
{
namespace _WatchdogTimedOut
{} // namespace _WatchdogTimedOut

struct WatchdogTimedOut
{
    static constexpr auto L = level::ERR;
    using metadata_types = std::tuple<>;
};

} // namespace Boot
} // namespace Host
} // namespace open_power
} // namespace org

namespace details
{

template <>
struct map_exception_type<
    sdbusplus::org::open_power::Host::Boot::Error::WatchdogTimedOut>
{
    using type = org::open_power::Host::Boot::WatchdogTimedOut;
};

} // namespace details

} // namespace logging

} // namespace phosphor

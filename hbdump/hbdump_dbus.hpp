#pragma once

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Logging/Create/server.hpp>

#include <string>

namespace dump
{
namespace hbdump
{

enum ReturnCodes
{
    RC_SUCCESS = 0,
    RC_NOT_HANDLED = 1,
    RC_DBUS_ERROR = 2
};

enum class HostRunningState
{
    Unknown,
    NotStarted,
    Started
};

using FFDCFormat =
    sdbusplus::xyz::openbmc_project::Logging::server::Create::FFDCFormat;

using FFDCTuple =
    std::tuple<FFDCFormat, uint8_t, uint8_t, sdbusplus::message::unix_fd>;

/**
 * Create a dbus method
 *
 * Find the dbus service associated with the dbus object path and create
 * a dbus method for calling the specified dbus interface and function.
 *
 * @param i_path - dbus object path
 * @param i_interface - dbus method interface
 * @param i_function - dbus interface function
 * @param o_method - method that is created
 * @param i_extended - optional for extended methods
 * @return non-zero if error
 *
 **/
int dbusMethod(const std::string& i_path, const std::string& i_interface,
               const std::string& i_function,
               sdbusplus::message::message& o_method,
               const std::string& i_extended = "");

/**
 * Create a PEL for the specified event type
 *
 * The additional data provided in the map will be placed in a user data
 * section of the PEL.
 *
 * @param  i_eventType - the event type
 * @param  i_additional - map of additional data
 * @param  i_ffdc - vector of ffdc data
 * @return Platform log id or 0 if error
 */
uint32_t createPel(const std::string& i_eventType,
                   std::map<std::string, std::string>& i_additional,
                   const std::vector<FFDCTuple>& i_ffdc);

/**
 * Get the host running state
 *
 * Use host boot progress to determine if a host has been started. If host
 * boot progress can not be determined then host state will be unknown.
 *
 * @return HostRunningState == "Unknown", "Started" or "NotStarted"
 */
HostRunningState hostRunningState();

} // namespace hbdump
} // namespace dump

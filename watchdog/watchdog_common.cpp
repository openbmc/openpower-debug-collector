#include "watchdog_common.hpp"

#include "watchdog_logging.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>

#include <map>

namespace watchdog
{
namespace dump
{

using namespace phosphor::logging;

void transitionHost(const std::string& target)
{
    constexpr auto systemdService = "org.freedesktop.systemd1";
    constexpr auto systemdObjPath = "/org/freedesktop/systemd1";
    constexpr auto systemdInterface = "org.freedesktop.systemd1.Manager";

    auto bus = sdbusplus::bus::new_system();
    auto method = bus.new_method_call(systemdService, systemdObjPath,
                                      systemdInterface, "StartUnit");

    method.append(target); // target unit to start
    method.append("replace");

    bus.call_noreply(method); // start the service
}

} // namespace dump
} // namespace watchdog

#include <libpdbg.h>

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <watchdog_common.hpp>
#include <watchdog_logging.hpp>

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

bool isAutoRebootEnabled()
{
    constexpr auto settingsService = "xyz.openbmc_project.Settings";
    constexpr auto settingsPath =
        "/xyz/openbmc_project/control/host0/auto_reboot";
    constexpr auto settingsIntf = "org.freedesktop.DBus.Properties";
    constexpr auto rebootPolicy =
        "xyz.openbmc_project.Control.Boot.RebootPolicy";

    auto bus = sdbusplus::bus::new_system();
    auto method =
        bus.new_method_call(settingsService, settingsPath, settingsIntf, "Get");

    method.append(rebootPolicy);
    method.append("AutoReboot");

    bool autoReboot = false;
    try
    {
        auto reply = bus.call(method);
        std::variant<bool> result;
        reply.read(result);
        autoReboot = std::get<bool>(result);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("Error in AutoReboot Get", entry("ERROR=%s", e.what()));
    }

    return autoReboot;
}

} // namespace dump
} // namespace watchdog

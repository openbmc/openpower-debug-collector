#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <phosphor-logging/log.hpp>
#include <watchdog_dbus.hpp>
#include <watchdog_logging.hpp>

#include <format>
#include <string>
#include <vector>

namespace watchdog
{
namespace dump
{

using namespace phosphor::logging;

int dbusMethod(const std::string& path, const std::string& interface,
               const std::string& function, sdbusplus::message_t& method,
               const std::string& extended)
{
    int rc = RC_DBUS_ERROR; // assume error

    try
    {
        constexpr auto serviceFind = "xyz.openbmc_project.ObjectMapper";
        constexpr auto pathFind = "/xyz/openbmc_project/object_mapper";
        constexpr auto interfaceFind = "xyz.openbmc_project.ObjectMapper";
        constexpr auto functionFind = "GetObject";

        auto bus = sdbusplus::bus::new_system(); // using system dbus

        // method to find service from object path and object interface
        auto newMethod = bus.new_method_call(serviceFind, pathFind,
                                             interfaceFind, functionFind);

        // find the service for specified object path and interface
        newMethod.append(path.c_str());
        newMethod.append(std::vector<std::string>({interface}));
        auto reply = bus.call(newMethod);

        // dbus call results
        std::map<std::string, std::vector<std::string>> responseFindService;
        reply.read(responseFindService);

        // If we successfully found the service associated with the dbus object
        // path and interface then create a method for the specified interface
        // and function.
        if (!responseFindService.empty())
        {
            auto service = responseFindService.begin()->first;

            // Some methods (e.g. get attribute) take an extended parameter
            if (extended == "")
            {
                // return the method
                method = bus.new_method_call(service.c_str(), path.c_str(),
                                             interface.c_str(),
                                             function.c_str());
            }
            else
            {
                // return extended method
                method = bus.new_method_call(service.c_str(), path.c_str(),
                                             extended.c_str(),
                                             function.c_str());
            }

            rc = RC_SUCCESS;
        }
        else
        {
            // This trace will be picked up in event log
            lg2::info("dbusMethod service not found");
            std::string traceMsgPath = std::string(path, maxTraceLen);
            lg2::info(traceMsgPath.c_str());
            std::string traceMsgIface = std::string(interface, maxTraceLen);
            lg2::info(traceMsgIface.c_str());
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Error in dbusMethod ({ERROR})", "ERROR", e);
    }

    return rc;
}

uint32_t createPel(const std::string& eventType,
                   std::map<std::string, std::string>& additional,
                   const std::vector<FFDCTuple>& ffdc)
{
    // Create returns plid
    int plid = 0;

    // Need to provide pid when using create or create-with-ffdc methods
    additional.emplace("_PID", std::to_string(getpid()));

    // Sdbus call specifics
    constexpr auto interface = "org.open_power.Logging.PEL";
    constexpr auto function = "CreatePELWithFFDCFiles";

    sdbusplus::message_t method;

    if (0 == dbusMethod(pathLogging, interface, function, method))
    {
        try
        {
            // append additional dbus call paramaters
            method.append(eventType, levelPelError, additional, ffdc);

            // using system dbus
            auto bus = sdbusplus::bus::new_system();
            auto response = bus.call(method);

            // reply will be tuple containing bmc log id, platform log id
            std::tuple<uint32_t, uint32_t> reply = {0, 0};

            // parse dbus response into reply
            response.read(reply);
            plid = std::get<1>(reply); // platform log id is tuple "second"
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error("Error in createPel CreatePELWithFFDCFiles ({ERROR})",
                       "ERROR", e);
        }
    }

    return plid; // platform log id or 0
}

bool isHostStateRunning()
{
    constexpr auto path = "/xyz/openbmc_project/state/host0";
    constexpr auto interface = "xyz.openbmc_project.State.Host";
    constexpr auto extended = "org.freedesktop.DBus.Properties";
    constexpr auto function = "Get";

    sdbusplus::message_t method;

    if (0 == dbusMethod(path, interface, function, method, extended))
    {
        try
        {
            method.append(interface, "CurrentHostState");
            auto bus = sdbusplus::bus::new_system();
            auto response = bus.call(method);
            std::variant<std::string> reply;

            response.read(reply);
            std::string currentHostState(std::get<std::string>(reply));

            if (currentHostState ==
                "xyz.openbmc_project.State.Host.HostState.Running")
            {
                return true;
            }
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error("Failed to read CurrentHostState property ({ERROR})",
                       "ERROR", e);
        }
    }

    return false;
}

} // namespace dump
} // namespace watchdog

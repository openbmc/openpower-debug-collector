#include <unistd.h>

#include <phosphor-logging/log.hpp>
#include <watchdog_dbus.hpp>
#include <watchdog_logging.hpp>
#include <xyz/openbmc_project/State/Boot/Progress/server.hpp>

#include <string>
#include <vector>

namespace watchdog
{
namespace dump
{

using namespace phosphor::logging;

int dbusMethod(const std::string& i_path, const std::string& i_interface,
               const std::string& i_function,
               sdbusplus::message::message& o_method,
               const std::string& i_extended)
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
        auto method = bus.new_method_call(serviceFind, pathFind, interfaceFind,
                                          functionFind);

        // find the service for specified object path and interface
        method.append(i_path.c_str());
        method.append(std::vector<std::string>({i_interface}));
        auto reply = bus.call(method);

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
            if (i_extended == "")
            {
                // return the method
                o_method = bus.new_method_call(service.c_str(), i_path.c_str(),
                                               i_interface.c_str(),
                                               i_function.c_str());
            }
            else
            {
                // return extended method
                o_method =
                    bus.new_method_call(service.c_str(), i_path.c_str(),
                                        i_extended.c_str(), i_function.c_str());
            }

            rc = RC_SUCCESS;
        }
        else
        {
            // This trace will be picked up in event log
            log<level::INFO>("dbusMethod service not found");
            std::string traceMsgPath = std::string(i_path, maxTraceLen);
            log<level::INFO>(traceMsgPath.c_str());
            std::string traceMsgIface = std::string(i_interface, maxTraceLen);
            log<level::INFO>(traceMsgIface.c_str());
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("Error in dbusMethod", entry("ERROR=%s", e.what()));
    }

    return rc;
}

uint32_t createPel(const std::string& i_eventType,
                   std::map<std::string, std::string>& i_additional,
                   const std::vector<FFDCTuple>& i_ffdc)
{
    // Create returns plid
    int plid = 0;

    // Need to provide pid when using create or create-with-ffdc methods
    i_additional.emplace("_PID", std::to_string(getpid()));

    // Sdbus call specifics
    constexpr auto interface = "org.open_power.Logging.PEL";
    constexpr auto function = "CreatePELWithFFDCFiles";

    sdbusplus::message::message method;

    if (0 == dbusMethod(pathLogging, interface, function, method))
    {
        try
        {
            // append additional dbus call paramaters
            method.append(i_eventType, levelPelError, i_additional, i_ffdc);

            // using system dbus
            auto bus = sdbusplus::bus::new_system();
            auto response = bus.call(method);

            // reply will be tuple containing bmc log id, platform log id
            std::tuple<uint32_t, uint32_t> reply = {0, 0};

            // parse dbus response into reply
            response.read(reply);
            plid = std::get<1>(reply); // platform log id is tuple "second"
        }
        catch (const sdbusplus::exception::SdBusError& e)
        {
            log<level::ERR>("Error in createPel CreatePELWithFFDCFiles",
                            entry("ERROR=%s", e.what()));
        }
    }

    return plid; // platform log id or 0
}

} // namespace dump
} // namespace watchdog

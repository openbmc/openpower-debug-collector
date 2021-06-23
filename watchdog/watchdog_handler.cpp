#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <watchdog_dbus.hpp>
#include <watchdog_handler.hpp>
#include <watchdog_logging.hpp>

#include <iomanip>
#include <iostream>

namespace watchdog
{
namespace dump
{

using namespace phosphor::logging;

/**
 * @brief Callback for dump request properties change signal monitor
 *
 * @param[in] i_msg - dbus message from the dbus match infrastructure
 * @param[in] i_path - the object path we are monitoring
 * @param[out] o_inProgress - used to break out of our dbus wait loop
 * @reutn Always non-zero indicating no error, no cascading callbacks
 */
uint dumpStatusChanged(sdbusplus::message::message& i_msg, std::string i_path,
                       bool& o_inProgress)
{
    // reply (msg) will be a property change message
    std::string interface;
    std::map<std::string, std::variant<std::string, uint8_t>> property;
    i_msg.read(interface, property);

    // looking for property Status changes
    std::string propertyType = "Status";
    auto dumpStatus = property.find(propertyType);

    if (dumpStatus != property.end())
    {
        const std::string* status =
            std::get_if<std::string>(&(dumpStatus->second));

        if ((nullptr != status) && ("xyz.openbmc_project.Common.Progress."
                                    "OperationStatus.InProgress" != *status))
        {
            // dump is done, trace some info and change in progress flag
            log<level::INFO>(i_path.c_str());
            log<level::INFO>((*status).c_str());
            o_inProgress = false;
        }
    }

    return 1; // non-negative return code for successful callback
}

/**
 * @brief Register a callback for dump progress status changes
 *
 * @param[in] i_path - the object path of the dump to monitor
 */
void monitorDump(const std::string& i_path)
{
    bool inProgress = true; // callback will update this

    // setup the signal match rules and callback
    std::string matchInterface = "xyz.openbmc_project.Common.Progress";
    auto bus = sdbusplus::bus::new_system();

    std::unique_ptr<sdbusplus::bus::match_t> match =
        std::make_unique<sdbusplus::bus::match_t>(
            bus,
            sdbusplus::bus::match::rules::propertiesChanged(
                i_path.c_str(), matchInterface.c_str()),
            [&](auto& msg) {
                return dumpStatusChanged(msg, i_path, inProgress);
            });

    // TODO: Add timeout if BootProgress stuck in InProgress
    // wait for dump status to be completed (complete == true)
    log<level::INFO>("hbdump requested");
    while (true == inProgress)
    {
        bus.wait(0);
        bus.process_discard();
    }
    log<level::INFO>("hbdump completed");
}

void requestDump(const uint32_t logId)
{
    constexpr auto path = "/org/openpower/dump";
    constexpr auto interface = "xyz.openbmc_project.Dump.Create";
    constexpr auto function = "CreateDump";

    sdbusplus::message::message method;

    if (0 == dbusMethod(path, interface, function, method))
    {
        try
        {
            // dbus call arguments
            std::map<std::string, std::string> createParams;
            createParams["com.ibm.Dump.Create.CreateParameters.DumpType"] =
                "com.ibm.Dump.Create.DumpType.Hostboot";
            createParams["com.ibm.Dump.Create.CreateParameters.ErrorLogId"] =
                std::to_string(logId);
            method.append(createParams);

            // using system dbus
            auto bus = sdbusplus::bus::new_system();
            auto response = bus.call(method);

            // reply will be type dbus::ObjectPath
            sdbusplus::message::object_path reply;
            response.read(reply);

            // monitor dump progress
            monitorDump(reply);
        }
        catch (const sdbusplus::exception::SdBusError& e)
        {
            log<level::ERR>("Error in requestDump",
                            entry("ERROR=%s", e.what()));
        }
    }
}

} // namespace dump
} // namespace watchdog

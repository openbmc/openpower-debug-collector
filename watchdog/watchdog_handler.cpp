#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <watchdog_dbus.hpp>
#include <watchdog_handler.hpp>
#include <watchdog_logging.hpp>

#include <format>

namespace watchdog
{
namespace dump
{

using namespace phosphor::logging;

/**
 * @brief Callback for dump request properties change signal monitor
 *
 * @param msg - dbus message from the dbus match infrastructure
 * @param path - the object path we are monitoring
 * @param progressStatus - dump progress status
 * @return Always non-zero indicating no error, no cascading callbacks
 */
uint dumpStatusChanged(sdbusplus::message_t& msg, std::string path,
                       DumpProgressStatus& progressStatus)
{
    // reply (msg) will be a property change message
    std::string interface;
    std::map<std::string, std::variant<std::string, uint8_t>> property;
    msg.read(interface, property);

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
            // dump is not in InProgress state, trace some info and change in
            // progress status
            log<level::INFO>(path.c_str());
            log<level::INFO>((*status).c_str());

            if ("xyz.openbmc_project.Common.Progress.OperationStatus."
                "Completed" == *status)
            {
                // Dump completed successfully
                progressStatus = DumpProgressStatus::Completed;
            }
            else
            {
                // Dump Failed
                progressStatus = DumpProgressStatus::Failed;
            }
        }
    }

    return 1; // non-negative return code for successful callback
}

/**
 * @brief Register a callback for dump progress status changes
 *
 * @param path - the object path of the dump to monitor
 * @param timeout - timeout - timeout interval in seconds
 */
void monitorDump(const std::string& path, const uint32_t timeout)
{
    // callback will update progressStatus
    DumpProgressStatus progressStatus = DumpProgressStatus::InProgress;

    // setup the signal match rules and callback
    std::string matchInterface = "xyz.openbmc_project.Common.Progress";
    auto bus = sdbusplus::bus::new_system();

    std::unique_ptr<sdbusplus::bus::match_t> match =
        std::make_unique<sdbusplus::bus::match_t>(
            bus,
            sdbusplus::bus::match::rules::propertiesChanged(
                path.c_str(), matchInterface.c_str()),
            [&](auto& msg) {
        return dumpStatusChanged(msg, path, progressStatus);
    });

    // wait for dump status to be completed (complete == true)
    // or until timeout interval

    bool timedOut = false;
    uint32_t secondsCount = 0;
    while ((DumpProgressStatus::InProgress == progressStatus) && !timedOut)
    {
        bus.wait(std::chrono::seconds(1));
        bus.process_discard();

        if (++secondsCount == timeout)
        {
            timedOut = true;
        }
    }

    if (timedOut)
    {
        log<level::ERR>("Dump progress status did not change to "
                        "complete within the timeout interval, exiting...");
    }
    else if (DumpProgressStatus::Completed == progressStatus)
    {
        log<level::INFO>("dump collection completed");
    }
    else
    {
        log<level::INFO>("dump collection failed");
    }
}

void requestDump(const DumpParameters& dumpParameters)
{
    constexpr auto path = "/org/openpower/dump";
    constexpr auto interface = "xyz.openbmc_project.Dump.Create";
    constexpr auto function = "CreateDump";

    sdbusplus::message_t method;

    if (0 == dbusMethod(path, interface, function, method))
    {
        try
        {
            // dbus call arguments
            std::map<std::string, std::variant<std::string, uint64_t>>
                createParams;
            createParams["com.ibm.Dump.Create.CreateParameters.ErrorLogId"] =
                uint64_t(dumpParameters.logId);
            if (DumpType::Hostboot == dumpParameters.dumpType)
            {
                log<level::INFO>("hostboot dump requested");
                createParams["com.ibm.Dump.Create.CreateParameters.DumpType"] =
                    "com.ibm.Dump.Create.DumpType.Hostboot";
            }
            else if (DumpType::SBE == dumpParameters.dumpType)
            {
                log<level::INFO>("SBE dump requested");
                createParams["com.ibm.Dump.Create.CreateParameters.DumpType"] =
                    "com.ibm.Dump.Create.DumpType.SBE";
                createParams
                    ["com.ibm.Dump.Create.CreateParameters.FailingUnitId"] =
                        dumpParameters.unitId;
            }

            method.append(createParams);

            // using system dbus
            auto bus = sdbusplus::bus::new_system();
            auto response = bus.call(method);

            // reply will be type dbus::ObjectPath
            sdbusplus::message::object_path reply;
            response.read(reply);

            // monitor dump progress
            monitorDump(reply, dumpParameters.timeout);
        }
        catch (const sdbusplus::exception_t& e)
        {
            constexpr auto ERROR_DUMP_DISABLED =
                "xyz.openbmc_project.Dump.Create.Error.Disabled";
            if (e.name() == ERROR_DUMP_DISABLED)
            {
                // Dump is disabled, Skip the dump collection.
                log<level::INFO>(
                    std::format(
                        "Dump is disabled on({}), skipping dump collection",
                        dumpParameters.unitId)
                        .c_str());
            }
            else
            {
                log<level::ERR>(
                    std::format("D-Bus call createDump exception ",
                                "OBJPATH={}, INTERFACE={}, EXCEPTION={}", path,
                                interface, e.what())
                        .c_str());
            }
        }
    }
}

} // namespace dump
} // namespace watchdog

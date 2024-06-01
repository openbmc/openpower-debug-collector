#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <watchdog_dbus.hpp>
#include <watchdog_handler.hpp>
#include <watchdog_logging.hpp>

namespace watchdog
{
namespace dump
{

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
            lg2::info("{PATH}", "PATH", path);
            lg2::info("{STATUS}", "STATUS", *status);

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
        lg2::error("Dump progress status did not change to "
                   "complete within the timeout interval, exiting...");
    }
    else if (DumpProgressStatus::Completed == progressStatus)
    {
        lg2::info("dump collection completed");
    }
    else
    {
        lg2::info("dump collection failed");
    }
}

void requestDump(const DumpParameters& dumpParameters)
{
    constexpr auto path = "/xyz/openbmc_project/dump/system";
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
                lg2::info("hostboot dump requested");
                createParams["com.ibm.Dump.Create.CreateParameters.DumpType"] =
                    "com.ibm.Dump.Create.DumpType.Hostboot";
            }
            else if (DumpType::SBE == dumpParameters.dumpType)
            {
                lg2::info("SBE dump requested");
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
                lg2::info("Dump is disabled on({UNIT}), "
                          "skipping dump collection",
                          "UNIT", dumpParameters.unitId);
            }
            else
            {
                lg2::error("D-Bus call createDump exception OBJPATH={OBJPATH}, "
                           "INTERFACE={INTERFACE}, EXCEPTION={ERROR}",
                           "OBJPATH", path, "INTERFACE", interface, "ERROR", e);
            }
        }
    }
}

} // namespace dump
} // namespace watchdog

#include "dump_utils.hpp"

#include <fmt/core.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>

#include <fstream>
#include <string>

namespace openpower
{
namespace dump
{
namespace util
{
using namespace phosphor::logging;
/**
 *  Callback for dump request properties change signal monitor
 *
 * @param[in] msg         Dbus message from the dbus match infrastructure
 * @param[in] path        The object path we are monitoring
 * @param[out] inProgress Used to break out of our dbus wait loop
 * @reutn Always non-zero indicating no error, no cascading callbacks
 */
uint32_t dumpStatusChanged(sdbusplus::message::message& msg,
                           const std::string& path, bool& inProgress)
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
            // dump is done, trace some info and change in progress flag
            log<level::INFO>(fmt::format("Dump status({}) : path={}",
                                         status->c_str(), path.c_str())
                                 .c_str());
            inProgress = false;
        }
    }

    return 1; // non-negative return code for successful callback
}

/**
 * Register a callback for dump progress status changes
 *
 * @param[in] path The object path of the dump to monitor
 * @param timeout - timeout - timeout interval in seconds
 */
void monitorDump(const std::string& path, const uint32_t timeout)
{
    auto inProgress = true; // callback will update this

    // setup the signal match rules and callback
    std::string matchInterface = "xyz.openbmc_project.Common.Progress";
    auto bus = sdbusplus::bus::new_system();

    std::unique_ptr<sdbusplus::bus::match_t> match =
        std::make_unique<sdbusplus::bus::match_t>(
            bus,
            sdbusplus::bus::match::rules::propertiesChanged(
                path.c_str(), matchInterface.c_str()),
            [&](auto& msg) {
                return dumpStatusChanged(msg, path, inProgress);
            });

    // wait for dump status to be completed (complete == true)
    // or until timeout interval
    log<level::INFO>("dump requested (waiting)");
    auto timedOut = false;
    uint32_t secondsCount = 0;
    while (inProgress && !timedOut)
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
}

void requestSBEDump(const uint32_t failingUnit, const uint32_t eid)
{
    log<level::INFO>(fmt::format("Requesting Dump PEL({}) chip position({})",
                                 eid, failingUnit)
                         .c_str());

    constexpr auto path = "/org/openpower/dump";
    constexpr auto interface = "xyz.openbmc_project.Dump.Create";
    constexpr auto function = "CreateDump";
    constexpr auto sbeDump = "com.ibm.Dump.Create.DumpType.SBE";

    sdbusplus::message::message method;

    auto bus = sdbusplus::bus::new_default();

    try
    {
        auto service = getService(bus, interface, path);
        auto method =
            bus.new_method_call(service.c_str(), path, interface, function);

        // dbus call arguments
        std::unordered_map<std::string, std::variant<std::string, uint64_t>>
            createParams;
        createParams["com.ibm.Dump.Create.CreateParameters.ErrorLogId"] =
            uint64_t(eid);
        createParams["com.ibm.Dump.Create.CreateParameters.DumpType"] = sbeDump;
        createParams["com.ibm.Dump.Create.CreateParameters.FailingUnitId"] =
            uint64_t(failingUnit);

        method.append(createParams);

        auto response = bus.call(method);

        // reply will be type dbus::ObjectPath
        sdbusplus::message::object_path reply;
        response.read(reply);

        // monitor dump progress
        monitorDump(reply, SBE_DUMP_TIMEOUT);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>(fmt::format("D-Bus call createDump exception",
                                    "OBJPATH={}, INTERFACE={}, EXCEPTION={}",
                                    path, interface, e.what())
                            .c_str());
        constexpr auto ERROR_DUMP_DISABLED =
            "xyz.openbmc_project.Dump.Create.Error.Disabled";
        if (e.name() == ERROR_DUMP_DISABLED)
        {
            // Dump is disabled, Skip the dump collection.
            log<level::INFO>(
                fmt::format("Dump is disabled on({}), skipping dump collection",
                            failingUnit)
                    .c_str());
        }
        else
        {
            throw;
        }
    }
    catch (const std::exception& e)
    {
        throw e;
    }
}

// Mapper

std::string getService(sdbusplus::bus::bus& bus, const std::string& intf,
                       const std::string& path)
{
    constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
    constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
    constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
    try
    {
        auto mapper = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetObject");

        mapper.append(path, std::vector<std::string>({intf}));

        auto mapperResponseMsg = bus.call(mapper);
        std::map<std::string, std::vector<std::string>> mapperResponse;
        mapperResponseMsg.read(mapperResponse);

        if (mapperResponse.empty())
        {
            log<level::ERR>(fmt::format("Empty mapper response for GetObject "
                                        "interface({}), path({})",
                                        intf, path)
                                .c_str());
            throw std::runtime_error("Empty mapper response for GetObject");
        }
        return mapperResponse.begin()->first;
    }
    catch (const sdbusplus::exception::exception& ex)
    {
        log<level::ERR>(fmt::format("Mapper call failed for GetObject "
                                    "errorMsg({}), path({}), interface({}) ",
                                    ex.what(), path, intf)
                            .c_str());
        throw;
    }
}

void prepareCollection(const std::filesystem::path& dumpPath,
                       const std::string& errorLogId)
{
    using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
    try
    {
        std::filesystem::create_directories(dumpPath);
        auto elogPath = dumpPath.parent_path();
        elogPath /= "ErrorLog";
        std::ofstream outfile{elogPath, std::ios::out | std::ios::binary};
        if (!outfile.good())
        {
            using metadata = xyz::openbmc_project::Common::File::Open;
            // Unable to open the file for writing
            auto err = errno;
            log<level::ERR>(
                fmt::format("Error opening file to write errorlog id, "
                            "errno({}), filepath({})",
                            err, dumpPath.string())
                    .c_str());
            // Report the error and continue collection even if the error log id
            // cannot be added
            report<Open>(metadata::ERRNO(err),
                         metadata::PATH(dumpPath.c_str()));
        }
        else
        {
            outfile.exceptions(std::ifstream::failbit | std::ifstream::badbit |
                               std::ifstream::eofbit);
            outfile << errorLogId;
            outfile.close();
        }
    }
    catch (std::filesystem::filesystem_error& e)
    {
        log<level::ERR>(fmt::format("Error creating dump directories, path({})",
                                    dumpPath.string())
                            .c_str());
        throw;
    }
    catch (std::ofstream::failure& oe)
    {
        using metadata = xyz::openbmc_project::Common::File::Write;
        // If there is an error commit the error and continue.
        log<level::ERR>(fmt::format("Failed to write errorlog id to file, "
                                    "errorMsg({}), error({}), filepath({})",
                                    oe.what(), oe.code().value(),
                                    dumpPath.string())
                            .c_str());
        // Report the error and continue with dump collection
        // even if the error log id cannot be written to the file.
        report<Write>(metadata::ERRNO(oe.code().value()),
                      metadata::PATH(dumpPath.c_str()));
    }
}

} // namespace util
} // namespace dump
} // namespace openpower

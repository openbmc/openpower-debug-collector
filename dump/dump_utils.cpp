#include "dump_utils.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>

#include <format>
#include <fstream>
#include <string>

namespace openpower::dump::util
{
using namespace phosphor::logging;

static void monitorDumpCreation(const std::string& path, const uint32_t timeout)
{
    bool inProgress = true;
    auto bus = sdbusplus::bus::new_system();
    auto match = sdbusplus::bus::match::match(
        bus,
        sdbusplus::bus::match::rules::propertiesChanged(
            path, "xyz.openbmc_project.Common.Progress"),
        [&](sdbusplus::message::message& msg) {
        std::string interface;
        std::map<std::string, std::variant<std::string, uint8_t>> property;
        msg.read(interface, property);

        const auto dumpStatus = property.find("Status");
        if (dumpStatus != property.end())
        {
            const std::string* status =
                std::get_if<std::string>(&(dumpStatus->second));
            if (status &&
                *status !=
                    "xyz.openbmc_project.Common.Progress.OperationStatus.InProgress")
            {
                lg2::info("Dump status({STATUS}) : path={PATH}", "STATUS",
                          status->c_str(), "PATH", path.c_str());
                inProgress = false;
            }
        }
    });

    // Timeout management
    for (uint32_t secondsCount = 0; inProgress && secondsCount < timeout;
         ++secondsCount)
    {
        bus.wait(std::chrono::seconds(1));
        bus.process_discard();
    }

    if (inProgress)
    {
        lg2::error("Dump progress timeout; dump may not be complete.");
    }
}

void requestSBEDump(const uint32_t failingUnit, const uint32_t eid,
                    SBETypes sbeType)
{
    lg2::info("Requesting Dump PEL({EID}) chip position({FAILINGUNIT})", "EID",
              eid, "FAILINGUNIT", failingUnit);

    auto path = sbeTypeAttributes.at(sbeType).dumpPath.c_str();
    constexpr auto interface = "xyz.openbmc_project.Dump.Create";
    constexpr auto function = "CreateDump";

    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto service = getService(bus, interface, path);
        auto method = bus.new_method_call(service.c_str(), path, interface,
                                          function);

        std::unordered_map<std::string, std::variant<std::string, uint64_t>>
            createParams = {
                {"com.ibm.Dump.Create.CreateParameters.ErrorLogId",
                 uint64_t(eid)},
                {"com.ibm.Dump.Create.CreateParameters.FailingUnitId",
                 uint64_t(failingUnit)}};

        method.append(createParams);
        sdbusplus::message::object_path reply;
        bus.call(method).read(reply);

        monitorDumpCreation(reply.str, SBE_DUMP_TIMEOUT);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        lg2::error("D-Bus call createDump exception OBJPATH={OBJPATH}, "
                   "INTERFACE={INTERFACE}, EXCEPTION={ERROR}",
                   "OBJPATH", path, "INTERFACE", interface, "ERROR", e);
        constexpr auto ERROR_DUMP_DISABLED =
            "xyz.openbmc_project.Dump.Create.Error.Disabled";
        if (e.name() == ERROR_DUMP_DISABLED)
        {
            // Dump is disabled, Skip the dump collection.
            lg2::info("Dump is disabled unit({FAILINGUNIT}), "
                      "skipping dump collection",
                      "FAILINGUNIT", failingUnit);
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
            lg2::error(
                "Empty mapper response for GetObject interface({INTERFACE}), "
                "path({PATH})",
                "INTERFACE", intf, "PATH", path);

            throw std::runtime_error("Empty mapper response for GetObject");
        }
        return mapperResponse.begin()->first;
    }
    catch (const sdbusplus::exception::exception& ex)
    {
        lg2::error(
            "Mapper call failed for GetObject errorMsg({ERROR}), path({PATH}),"
            "interface({INTERFACE})",
            "ERROR", ex, "PATH", path, "INTERFACE", intf);

        throw;
    }
}

} // namespace openpower::dump::util

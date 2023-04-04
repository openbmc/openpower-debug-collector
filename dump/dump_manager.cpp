#include "config.h"

#include "dump_manager.hpp"

#include "dump_utils.hpp"
#include "sbe_consts.hpp"

#include <fmt/core.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdeventplus/exception.hpp>
#include <sdeventplus/source/base.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Dump/Create/error.hpp>

namespace openpower
{
namespace dump
{

constexpr auto DUMP_CREATE_IFACE = "xyz.openbmc_project.Dump.Create";
// constexpr auto MAX_ERROR_LOG_ID = 0xFFFFFFFF;

// Maximum possible processors in an OpenPOWER system
// constexpr auto MAX_FAILING_UNIT = 0x20;
constexpr auto ERROR_DUMP_DISABLED =
    "xyz.openbmc_project.Dump.Create.Error.Disabled";
constexpr auto ERROR_DUMP_QUOTA_EXCEEDED =
    "xyz.openbmc_project.Dump.Create.Error.QuotaExceeded";
constexpr auto ERROR_DUMP_NOT_ALLOWED =
    "xyz.openbmc_project.Common.Error.NotAllowed";

std::unordered_map<std::string, std::string> dumpTypeMap = {
    {"com.ibm.Dump.Create.DumpType.Hostboot", HB_DUMP_DBUS_OBJPATH},
    {"com.ibm.Dump.Create.DumpType.Hardware", HW_DUMP_DBUS_OBJPATH},
    {"com.ibm.Dump.Create.DumpType.SBE", SBE_DUMP_DBUS_OBJPATH}};

sdbusplus::message::object_path
    Manager::createDump(DumpCreateParams createParams)
{
    using namespace phosphor::logging;
    sdbusplus::message::object_path newDumpPath;
    auto dumpPath = extractDumpPath(createParams);
    log<level::INFO>(
        fmt::format("Request to collect dump({})", dumpPath).c_str());
    try
    {
        // Pass empty create parameters since no additional parameters
        // are needed.
        auto dumpManager = util::getService(bus, DUMP_CREATE_IFACE, dumpPath);
        auto method = bus.new_method_call(dumpManager.c_str(), dumpPath.c_str(),
                                          DUMP_CREATE_IFACE, "CreateDump");
        method.append(createParams);
        auto response = bus.call(method);
        response.read(newDumpPath);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(
            fmt::format("D-Bus call exception, errorMsg({})", e.what())
                .c_str());
        if (e.name() == ERROR_DUMP_DISABLED)
        {
            elog<sdbusplus::xyz::openbmc_project::Dump::Create::Error::
                     Disabled>();
        }
        if (e.name() == ERROR_DUMP_QUOTA_EXCEEDED)
        {
            using DumpQuotaExceeded = sdbusplus::xyz::openbmc_project::Dump::
                Create::Error::QuotaExceeded;
            using Reason =
                xyz::openbmc_project::Dump::Create::QuotaExceeded::REASON;
            elog<DumpQuotaExceeded>(Reason(e.description()));
        }
        if (e.name() == ERROR_DUMP_NOT_ALLOWED)
        {
            using DumpNotAllowed =
                sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;
            using Reason = xyz::openbmc_project::Common::NotAllowed::REASON;
            elog<DumpNotAllowed>(Reason(e.description()));
        }
        else
        {
            // re-throw exception
            throw;
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(
            fmt::format("Failed to get dump id,invalid dump entry path({})",
                        std::string(newDumpPath))
                .c_str());
        throw;
    }
    return newDumpPath;
}

std::string Manager::extractDumpPath(DumpCreateParams& params)
{
    using namespace phosphor::logging;
    using InvalidArgument =
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
    using CreateParameters =
        sdbusplus::com::ibm::Dump::server::Create::CreateParameters;
    using Argument = xyz::openbmc_project::Common::InvalidArgument;

    // Get dump type
    auto iter = params.find(
        sdbusplus::com::ibm::Dump::server::Create::
            convertCreateParametersToString(CreateParameters::DumpType));
    if (iter == params.end())
    {
        log<level::ERR>("Required argument, dump type is missing");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("DUMP_TYPE"),
                              Argument::ARGUMENT_VALUE("MISSING"));
    }

    if (!std::holds_alternative<std::string>(iter->second))
    {
        log<level::ERR>("Invalid argument value is passed for dump type");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("DUMP_TYPE"),
                              Argument::ARGUMENT_VALUE("INVALID"));
    }

    auto dumpTypeStr = std::get<std::string>(iter->second);
    params.erase(iter);

    auto dumpTypeIter = dumpTypeMap.find(dumpTypeStr);
    if (dumpTypeIter == dumpTypeMap.end())
    {
        log<level::ERR>(
            fmt::format("Invalid dump type passed dumpType({}) error",
                        dumpTypeStr)
                .c_str());
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("DUMP_TYPE"),
                              Argument::ARGUMENT_VALUE(dumpTypeStr.c_str()));
    }

    return dumpTypeIter->second;
}

} // namespace dump
} // namespace openpower

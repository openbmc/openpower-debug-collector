#include "dump_manager.hpp"

#include "dump_utils.hpp"
#include "sbe_consts.hpp"

#include <fmt/core.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Dump/Create/error.hpp>

namespace openpower
{
namespace dump
{

constexpr auto DUMP_CREATE_IFACE = "xyz.openbmc_project.Dump.Create";
constexpr auto MAX_ERROR_LOG_ID = 0xFFFFFFFF;

// Maximum possible processors in an OpenPOWER system
constexpr auto MAX_FAILING_UNIT = 0x20;
constexpr auto ERROR_DUMP_DISABLED =
    "xyz.openbmc_project.Dump.Create.Error.Disabled";

/* @struct DumpTypeInfo
 * @brief to store basic info about different dump types
 */
struct DumpTypeInfo
{
    std::string dumpPath;           // D-Bus path of the dump
    std::string dumpCollectionPath; // Path were dumps are stored
};
/* Map of dump type to the basic info of the dumps */
std::map<uint8_t, DumpTypeInfo> dumpInfo = {};

std::unordered_map<std::string, uint8_t> dumpTypeMap = {
    {"com.ibm.Dump.Create.DumpType.Hostboot", SBE::SBE_DUMP_TYPE_HOSTBOOT},
    {"com.ibm.Dump.Create.DumpType.Hardware", SBE::SBE_DUMP_TYPE_HARDWARE},
    {"com.ibm.Dump.Create.DumpType.SBE", SBE::SBE_DUMP_TYPE_SBE}};

sdbusplus::message::object_path Manager::createDumpEntry(DumpParams& dparams)
{
    using namespace phosphor::logging;
    using DumpDisabled =
        sdbusplus::xyz::openbmc_project::Dump::Create::Error::Disabled;
    sdbusplus::message::object_path newDumpPath;
    try
    {
        // Pass empty create parameters since no additional parameters
        // are needed.
        util::DumpCreateParams createDumpParams;
        auto dumpManager = util::getService(
            bus, DUMP_CREATE_IFACE, dumpInfo[dparams.dumpType].dumpPath);
        auto method = bus.new_method_call(
            dumpManager.c_str(), dumpInfo[dparams.dumpType].dumpPath.c_str(),
            DUMP_CREATE_IFACE, "CreateDump");
        method.append(createDumpParams);
        auto response = bus.call(method);
        response.read(newDumpPath);

        // DUMP Path format /xyz/openbmc_project/dump/<dump_type>/entry/<id>
        dparams.id = std::stoi(newDumpPath.filename());
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(
            fmt::format("D-Bus call exception, errorMsg({})", e.what())
                .c_str());
        if (e.name() == ERROR_DUMP_DISABLED)
        {
            elog<DumpDisabled>();
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

void Manager::getParams(const DumpCreateParams& params, DumpParams& dparams)
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

    auto dumpType = std::get<std::string>(iter->second);

    auto dumpTypeIter = dumpTypeMap.find(dumpType);
    if (dumpTypeIter == dumpTypeMap.end())
    {
        log<level::ERR>(
            fmt::format("Invalid dump type passed dumpType({}) error", dumpType)
                .c_str());
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("DUMP_TYPE"),
                              Argument::ARGUMENT_VALUE(dumpType.c_str()));
    }

    dparams.dumpType = dumpTypeIter->second;

    // get error log id
    iter = params.find(
        sdbusplus::com::ibm::Dump::server::Create::
            convertCreateParametersToString(CreateParameters::ErrorLogId));
    if (iter == params.end())
    {
        log<level::ERR>("Required argument, error log id is not passed");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("ERROR_LOG_ID"),
                              Argument::ARGUMENT_VALUE("MISSING"));
    }

    if (!std::holds_alternative<uint64_t>(iter->second))
    {
        // Exception will be raised if the input is not uint64
        log<level::ERR>("An invalid error log id is passed, setting as 0");
        report<InvalidArgument>(Argument::ARGUMENT_NAME("ERROR_LOG_ID"),
                                Argument::ARGUMENT_VALUE("INVALID INPUT"));
    }

    dparams.eid = std::get<uint64_t>(iter->second);

    if (dparams.eid > MAX_ERROR_LOG_ID)
    {
        // An error will be logged if the error log id is larger than maximum
        // value and set the error log id as 0.
        log<level::ERR>(fmt::format("Error log id is greater than maximum({}) "
                                    "length, setting as 0, errorid({})",
                                    MAX_ERROR_LOG_ID, dparams.eid)
                            .c_str());
        report<InvalidArgument>(
            Argument::ARGUMENT_NAME("ERROR_LOG_ID"),
            Argument::ARGUMENT_VALUE(std::to_string(dparams.eid).c_str()));
        dparams.eid = 0;
    }

    if ((dparams.dumpType == SBE::SBE_DUMP_TYPE_HARDWARE) ||
        (dparams.dumpType == SBE::SBE_DUMP_TYPE_SBE))
    {
        iter = params.find(sdbusplus::com::ibm::Dump::server::Create::
                               convertCreateParametersToString(
                                   CreateParameters::FailingUnitId));
        if (iter == params.end())
        {
            log<level::ERR>("Required argument, failing unit id is not passed");
            elog<InvalidArgument>(Argument::ARGUMENT_NAME("FAILING_UNIT_ID"),
                                  Argument::ARGUMENT_VALUE("MISSING"));
        }

        if (!std::holds_alternative<uint64_t>(iter->second))
        {
            // Exception will be raised if the input is not uint64
            log<level::ERR>("An invalid failing unit id is passed ");
            report<InvalidArgument>(Argument::ARGUMENT_NAME("FAILING_UNIT_ID"),
                                    Argument::ARGUMENT_VALUE("INVALID INPUT"));
        }
        dparams.failingUnit = std::get<uint64_t>(iter->second);

        if (dparams.failingUnit > MAX_FAILING_UNIT)
        {
            log<level::ERR>(fmt::format("Invalid failing uint id: greater than "
                                        "maximum number({}): input({})",
                                        dparams.failingUnit, MAX_FAILING_UNIT)
                                .c_str());
            elog<InvalidArgument>(
                Argument::ARGUMENT_NAME("FAILING_UNIT_ID"),
                Argument::ARGUMENT_VALUE(
                    std::to_string(dparams.failingUnit).c_str()));
        }
    }
}

sdbusplus::message::object_path
    Manager::createDump(DumpCreateParams createParams)
{
    using namespace phosphor::logging;
    DumpParams dumpParams;
    getParams(createParams, dumpParams);
    log<level::INFO>(
        fmt::format(
            "Request to collect dump type({}), eid({}), failingUnit({})",
            dumpParams.dumpType, dumpParams.eid, dumpParams.failingUnit)
            .c_str());

    return createDumpEntry(dumpParams);
}
} // namespace dump
} // namespace openpower

#include "config.h"

#include "dump_manager.hpp"

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
constexpr auto MAX_FAILING_UNIT = 0x20;
constexpr auto ERROR_DUMP_DISABLED =
    "xyz.openbmc_project.Dump.Create.Error.Disabled";
constexpr auto OP_SBE_FILES_PATH = "plat_dump";
constexpr auto DUMP_NOTIFY_IFACE = "xyz.openbmc_project.Dump.NewDump";
constexpr auto DUMP_PROGRESS_IFACE = "xyz.openbmc_project.Common.Progress";
constexpr auto STATUS_PROP = "Status";

/* @struct DumpTypeInfo
 * @brief to store basic info about different dump types
 */
struct DumpTypeInfo
{
    std::string dumpPath;           // D-Bus path of the dump
    std::string dumpCollectionPath; // Path were dumps are stored
};

/* Map of dump type to the basic info of the dumps */
std::map<uint8_t, DumpTypeInfo> dumpInfo = {
    {SBE::SBE_DUMP_TYPE_HOSTBOOT,
     {HB_DUMP_DBUS_OBJPATH, HB_DUMP_COLLECTION_PATH}},
    {SBE::SBE_DUMP_TYPE_HARDWARE,
     {HW_DUMP_DBUS_OBJPATH, HW_DUMP_COLLECTION_PATH}}};

std::map<std::string, uint8_t> dumpTypeMap = {
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
    // DUMP Path format /xyz/openbmc_project/dump/<dump_type>/entry/<id>
    try
    {
        dparams.id = std::stoi(newDumpPath.filename());
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

void Manager::getParams(const util::DumpCreateParams& params,
                        DumpParams& dparams)
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
        log<level::ERR>("Required argument, dump type is not passed");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("DUMP_TYPE"),
                              Argument::ARGUMENT_VALUE("MISSING"));
    }

    try
    {
        dparams.dumpType = dumpTypeMap.at(std::get<std::string>(iter->second));
    }
    catch (const std::exception& e)
    {
        std::string dumpType = std::get<std::string>(iter->second);
        log<level::ERR>(
            fmt::format("Invalid dump type passed dumpType({}) error", dumpType)
                .c_str());
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("DUMP_TYPE"),
                              Argument::ARGUMENT_VALUE(dumpType.c_str()));
    }

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

    try
    {
        dparams.eid = std::get<uint64_t>(iter->second);
    }
    catch (const std::bad_variant_access& e)
    {
        // Exception will be raised if the input is not uint64
        auto err = errno;
        log<level::ERR>(fmt::format("An ivalid error log id is passed, setting "
                                    "as 0, errorMsg({}), errno({}), error({})",
                                    e.what(), err, strerror(err))
                            .c_str());
        report<InvalidArgument>(Argument::ARGUMENT_NAME("ERROR_LOG_ID"),
                                Argument::ARGUMENT_VALUE("INVALID INPUT"));
    }

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

        try
        {
            dparams.failingUnit = std::get<uint64_t>(iter->second);
        }
        catch (const std::bad_variant_access& e)
        {
            // Exception will be raised if the input is not uint64
            auto err = errno;
            log<level::ERR>(
                fmt::format("An invalid failing unit id is passed "
                            "errorMsg({}), errno({}), errorString({})",
                            e.what(), err, strerror(err))
                    .c_str());
            elog<InvalidArgument>(Argument::ARGUMENT_NAME("FAILING_UNIT_ID"),
                                  Argument::ARGUMENT_VALUE("INVALID INPUT"));
        }

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
    Manager::createDump(util::DumpCreateParams createParams)
{
    using namespace phosphor::logging;
    DumpParams dumpParams;
    getParams(createParams, dumpParams);
    log<level::INFO>(
        fmt::format(
            "Request to collect dump type({}), eid({}), failingUnit({})",
            dumpParams.dumpType, dumpParams.eid, dumpParams.failingUnit)
            .c_str());

    auto dumpEntry = createDumpEntry(dumpParams);

    pid_t pid = fork();
    if (pid == 0)
    {
        std::filesystem::path dumpPath(
            dumpInfo[dumpParams.dumpType].dumpCollectionPath);
        dumpPath /= std::to_string(dumpParams.id);
        dumpPath /= OP_SBE_FILES_PATH;
        util::prepareCollection(dumpPath, std::to_string(dumpParams.eid));
    }
    else if (pid < 0)
    {
        // Fork failed
        log<level::ERR>("Failure in fork call");
        throw std::runtime_error("Failure in fork call");
    }
    else
    {
        using InternalFailure =
            sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
        using namespace sdeventplus::source;
        Child::Callback callback = [this, dumpParams,
                                    dumpEntry](Child& eventSource,
                                               const siginfo_t* si) {
            eventSource.set_enabled(Enabled::On);
            if (si->si_status == 0)
            {
                log<level::INFO>("Dump collected, initiating packaging");
                auto dumpManager =
                    util::getService(bus, DUMP_NOTIFY_IFACE,
                                     dumpInfo[dumpParams.dumpType].dumpPath);
                auto method = bus.new_method_call(
                    dumpManager.c_str(),
                    dumpInfo[dumpParams.dumpType].dumpPath.c_str(),
                    DUMP_NOTIFY_IFACE, "Notify");
                method.append(static_cast<uint32_t>(dumpParams.id),
                              static_cast<uint64_t>(0));
                bus.call_noreply(method);
            }
            else
            {
                log<level::ERR>("Dump collection failed, updating status");
                util::setProperty(DUMP_PROGRESS_IFACE, STATUS_PROP, dumpEntry,
                                  bus,
                                  std::variant<std::string>(
                                      "xyz.openbmc_project.Common.Progress."
                                      "OperationStatus.Failed"));
            }
        };
        try
        {
            sigset_t ss;
            if (sigemptyset(&ss) < 0)
            {
                log<level::ERR>("Unable to initialize signal set");
                elog<InternalFailure>();
            }
            if (sigaddset(&ss, SIGCHLD) < 0)
            {
                log<level::ERR>("Unable to add signal to signal set");
                elog<InternalFailure>();
            }
            // Block SIGCHLD first, so that the event loop can handle it
            if (sigprocmask(SIG_BLOCK, &ss, NULL) < 0)
            {
                log<level::ERR>("Unable to block signal");
                elog<InternalFailure>();
            }
            if (childPtr)
            {
                childPtr.reset();
            }
            childPtr = std::make_unique<Child>(event, pid, WEXITED | WSTOPPED,
                                               std::move(callback));
        }
        catch (const InternalFailure& e)
        {
            commit<InternalFailure>();
        }
        catch (const sdbusplus::exception::exception& e)
        {
            log<level::ERR>(
                fmt::format("Unable to update the dump status, errorMsg({})",
                            e.what())
                    .c_str());
            commit<InternalFailure>();
        }
    }

    return dumpEntry;
}
} // namespace dump
} // namespace openpower

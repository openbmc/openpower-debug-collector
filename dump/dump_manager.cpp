extern "C"
{
#include <libpdbg_sbe.h>
}

#include "config.h"

#include "attributes_info.H"

#include "dump_manager.hpp"
#include "dump_utils.hpp"
#include "sbe_consts.hpp"

#include <fmt/core.h>
#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <chrono>
#include <fstream>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

namespace openpower
{
namespace dump
{
using namespace phosphor::logging;
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

constexpr auto DUMP_CREATE_IFACE = "xyz.openbmc_project.Dump.Create";
constexpr auto DUMP_NOTIFY_IFACE = "xyz.openbmc_project.Dump.NewDump";
constexpr auto DUMP_PROGRESS_IFACE = "xyz.openbmc_project.Common.Progress";
constexpr auto STATUS_PROP = "Status";
constexpr auto OP_SBE_FILES_PATH = "plat_dump";
constexpr auto MAX_ERROR_LOG_ID = 0xFFFFFFFF;
constexpr auto INVALID_FAILING_UNIT = 0xFF;

// Maximum 32 processors are possible in a system.
constexpr auto MAX_FAILING_UNIT = 0x20;

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

bool Manager::isMasterProc(struct pdbg_target* proc) const
{
    ATTR_PROC_MASTER_TYPE_Type type;
    // Get processor type (Primary or Alt-primary)
    if (DT_GET_PROP(ATTR_PROC_MASTER_TYPE, proc, type))
    {
        log<level::ERR>("Attribute [ATTR_PROC_MASTER_TYPE] get failed");
        throw std::runtime_error(
            "Attribute [ATTR_PROC_MASTER_TYPE] get failed");
    }

    // Attribute value 0 corresponds to primary processor
    if (type == ENUM_ATTR_PROC_MASTER_TYPE_ACTING_MASTER)
    {
        return true;
    }
    return false;
}

void Manager::collectDumpFromSBE(struct pdbg_target* proc,
                                 std::filesystem::path& dumpPath,
                                 const uint32_t id, const uint8_t type,
                                 const uint8_t clockState,
                                 const uint8_t chipPos,
                                 const uint8_t collectFastArray)
{
    using namespace sdbusplus::xyz::openbmc_project::Common::Error;
    namespace fileError = sdbusplus::xyz::openbmc_project::Common::File::Error;

    struct pdbg_target* pib = NULL;
    pdbg_for_each_target("pib", proc, pib)
    {
        ATTR_HWAS_STATE_Type hwasState;
        if (DT_GET_PROP(ATTR_HWAS_STATE, pib, hwasState))
        {
            log<level::ERR>("Attribute [ATTR_HWAS_STATE] get failed");
            throw std::runtime_error("Attribute [ATTR_HWAS_STATE] get failed");
        }
        // If the pib is not functional skip
        if (!hwasState.functional)
        {
            continue;
        }
        break;
    }

    if (pib == NULL)
    {
        log<level::ERR>(fmt::format("No valid PIB target found dump type({}), "
                                    "clockstate({}), proc position({}",
                                    type, clockState, chipPos)
                            .c_str());
        throw std::runtime_error("No valid pib target found");
    }

    // TODO #ibm-openbmc/dev/3039
    // Check the SBE state before attempt the dump collection
    // Skip this SBE if the SBE is not in a good state.

    int error = 0;
    DumpDataPtr dataPtr;
    uint32_t len = 0;

    log<level::INFO>(
        fmt::format(
            "Collecting dump type({}), clockstate({}), proc position({})", type,
            clockState, chipPos)
            .c_str());
    if ((error = sbe_dump(pib, type, clockState, collectFastArray,
                          dataPtr.getPtr(), &len)) < 0)
    {
        // Add a trace if the failure is on the secondary.
        if ((!isMasterProc(proc)) && (type == SBE::SBE_DUMP_TYPE_HOSTBOOT))
        {
            log<level::ERR>(
                fmt::format("Error in collecting dump from "
                            "secondary SBE, chip_position({}) skipping",
                            chipPos)
                    .c_str());
            return;
        }
        log<level::ERR>(
            fmt::format("Failed to collect dump, chip_position({})", chipPos)
                .c_str());
        // TODO Create a PEL in the future for this failure case.
        throw std::system_error(error, std::generic_category(),
                                "Failed to collect dump from SBE");
    }

    if (len == 0)
    {
        // Add a trace if no data from secondary
        if ((!isMasterProc(proc)) && (type == SBE::SBE_DUMP_TYPE_HOSTBOOT))
        {
            log<level::INFO>(
                fmt::format("No hostboot dump recieved from secondary SBE, "
                            "skipping, chip_position({})",
                            chipPos)
                    .c_str());
            return;
        }
        log<level::ERR>(
            fmt::format(
                "No data returned while collecting the dump chip_position({})",
                chipPos)
                .c_str());
        throw std::runtime_error("No data returned while collecting the dump");
    }

    // Filename format: <dump_id>.SbeDataClocks<On/Off>.node0.proc<number>
    std::stringstream ss;
    ss << std::setw(8) << std::setfill('0') << id;

    std::string clockStr = (clockState == SBE::SBE_CLOCK_ON) ? "On" : "Off";

    // Assuming only node0 is supported now
    std::string filename = ss.str() + ".SbeDataClocks" + clockStr +
                           ".node0.proc" + std::to_string(chipPos);
    dumpPath /= filename;

    std::ofstream outfile{dumpPath, std::ios::out | std::ios::binary};
    if (!outfile.good())
    {
        using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
        using metadata = xyz::openbmc_project::Common::File::Open;
        // Unable to open the file for writing
        auto err = errno;
        log<level::ERR>(
            fmt::format(
                "Error opening file to write dump, errno({}), filepath({})",
                err, dumpPath.string())
                .c_str());
        report<Open>(metadata::ERRNO(err), metadata::PATH(dumpPath.c_str()));
        // Just return here, so that the dumps collected from other
        // SBEs can be packaged.
        return;
    }

    outfile.exceptions(std::ifstream::failbit | std::ifstream::badbit |
                       std::ifstream::eofbit);

    try
    {
        outfile.write(reinterpret_cast<char*>(dataPtr.getData()), len);
        outfile.close();
    }
    catch (std::ofstream::failure& oe)
    {
        using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
        using metadata = xyz::openbmc_project::Common::File::Write;
        log<level::ERR>(fmt::format("Failed to write to dump file, "
                                    "errorMsg({}), error({}), filepath({})",
                                    oe.what(), oe.code().value(),
                                    dumpPath.string())
                            .c_str());
        report<Write>(metadata::ERRNO(oe.code().value()),
                      metadata::PATH(dumpPath.c_str()));
        // Just return here so dumps collected from other SBEs can be
        // packaged.
        return;
    }
    log<level::INFO>(
        fmt::format("Dump collected type({}), clockstate({}), position({})",
                    type, clockState, chipPos)
            .c_str());
}

void Manager::collectSBEDump(std::filesystem::path& dumpPath, uint32_t id,
                             const uint8_t chipPos)
{
    log<level::INFO>(
        fmt::format("Collecting SBE dump: path({}), id({}), chip position({})",
                    dumpPath.string(), id, chipPos)
            .c_str());
    std::exit(EXIT_FAILURE);
}

void Manager::collectDump(uint8_t type, uint32_t id, std::string errorLogId,
                          const uint64_t failingUnit)
{
    std::filesystem::path dumpPath(dumpInfo[type].dumpCollectionPath);
    dumpPath /= std::to_string(id);
    std::filesystem::path sbeFilePath = dumpPath / OP_SBE_FILES_PATH;
    try
    {
        std::filesystem::create_directories(sbeFilePath);
    }
    catch (std::filesystem::filesystem_error& e)
    {
        log<level::ERR>(fmt::format("Error creating dump directories, path({})",
                                    sbeFilePath.string())
                            .c_str());
        report<InternalFailure>();
        std::exit(EXIT_FAILURE);
    }

    std::filesystem::path elogPath = dumpPath / "ErrorLog";
    std::ofstream outfile{elogPath, std::ios::out | std::ios::binary};
    if (!outfile.good())
    {
        using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
        using metadata = xyz::openbmc_project::Common::File::Open;
        // Unable to open the file for writing
        auto err = errno;
        log<level::ERR>(fmt::format("Error opening file to write errorlog id, "
                                    "errno({}), filepath({})",
                                    err, dumpPath.string())
                            .c_str());
        // Report the error and continue collection even if the error log id
        // cannot be added
        report<Open>(metadata::ERRNO(err), metadata::PATH(dumpPath.c_str()));
    }
    else
    {
        outfile.exceptions(std::ifstream::failbit | std::ifstream::badbit |
                           std::ifstream::eofbit);
        try
        {
            outfile << errorLogId;
            outfile.close();
        }
        catch (std::ofstream::failure& oe)
        {
            using namespace sdbusplus::xyz::openbmc_project::Common::File::
                Error;
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

    if (type == SBE::SBE_DUMP_TYPE_SBE)
    {
        collectSBEDump(sbeFilePath, id, failingUnit);
        return;
    }

    struct pdbg_target* target;
    bool failed = false;

    if (!pdbg_targets_init(NULL))
    {
        log<level::ERR>("pdbg_targets_init failed");
        throw std::runtime_error("pdbg target initialization failed");
    }
    pdbg_set_loglevel(PDBG_INFO);

    std::vector<uint8_t> clockStates = {SBE::SBE_CLOCK_ON, SBE::SBE_CLOCK_OFF};
    for (auto cstate : clockStates)
    {
        std::vector<pid_t> pidList;
        pdbg_for_each_class_target("proc", target)
        {
            if (pdbg_target_probe(target) != PDBG_TARGET_ENABLED)
            {
                continue;
            }

            ATTR_HWAS_STATE_Type hwasState;
            if (DT_GET_PROP(ATTR_HWAS_STATE, target, hwasState))
            {
                log<level::ERR>("Attribute [ATTR_HWAS_STATE] get failed");
                throw std::runtime_error(
                    "Attribute [ATTR_HWAS_STATE] get failed");
            }
            // If the proc is not functional skip
            if (!hwasState.functional)
            {
                if (isMasterProc(target))
                {
                    // Primary processor is not functional
                    log<level::INFO>("Primary Processor is not functional");
                }
                continue;
            }

            uint32_t chipPos = 0;
            if (DT_GET_PROP(ATTR_FAPI_POS, target, chipPos))
            {
                log<level::ERR>("Attribute [ATTR_FAPI_POS] get failed");
                throw std::runtime_error(
                    "Attribute [ATTR_FAPI_POS] get failed");
                continue;
            }

            uint8_t collectFastArray = 0;
            if (cstate == SBE::SBE_CLOCK_OFF)
            {
                if ((type == SBE::SBE_DUMP_TYPE_HOSTBOOT) ||
                    ((type == SBE::SBE_DUMP_TYPE_HARDWARE) &&
                     (chipPos == failingUnit)))
                {
                    collectFastArray = 1;
                }
            }

            pid_t pid = fork();

            if (pid < 0)
            {
                log<level::ERR>(
                    "Fork failed while starting hostboot dump collection");
                report<InternalFailure>();
                // Attempt the collection from next SBE.
                continue;
            }
            else if (pid == 0)
            {
                try
                {
                    collectDumpFromSBE(target, sbeFilePath, id, type, cstate,
                                       chipPos, collectFastArray);
                }
                catch (const std::runtime_error& error)
                {
                    log<level::ERR>(
                        fmt::format(
                            "Failed to execute collection, errorMsg({})",
                            error.what())
                            .c_str());
                    std::exit(EXIT_FAILURE);
                }
                std::exit(EXIT_SUCCESS);
            }
            else
            {
                pidList.push_back(std::move(pid));
            }
        }

        for (auto& p : pidList)
        {
            int status = 0;
            waitpid(p, &status, 0);
            if (WEXITSTATUS(status))
            {
                log<level::ERR>(
                    fmt::format("Dump collection failed, status({}) pid({})",
                                status, p)
                        .c_str());
                failed = true;
            }
        }

        // Exit if there is a critical failure and collection cannot continue
        if (failed)
        {
            log<level::ERR>("Failed to collect the dump");
            std::exit(EXIT_FAILURE);
        }
        log<level::INFO>(
            fmt::format("Dump collection completed for clock_state({})", cstate)
                .c_str());
    }
}

sdbusplus::message::object_path Manager::createDump(DumpCreateParams params)
{
    using namespace phosphor::logging;
    using InvalidArgument =
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
    using CreateParameters =
        sdbusplus::com::ibm::Dump::server::Create::CreateParameters;
    using Argument = xyz::openbmc_project::Common::InvalidArgument;

    auto iter = params.find(
        sdbusplus::com::ibm::Dump::server::Create::
            convertCreateParametersToString(CreateParameters::DumpType));
    if (iter == params.end())
    {
        log<level::ERR>("Required argument, dump type is not passed");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("DUMP_TYPE"),
                              Argument::ARGUMENT_VALUE("MISSING"));
    }
    std::string dumpType = std::get<std::string>(iter->second);

    iter = params.find(
        sdbusplus::com::ibm::Dump::server::Create::
            convertCreateParametersToString(CreateParameters::ErrorLogId));
    if (iter == params.end())
    {
        log<level::ERR>("Required argument, error log id is not passed");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("ERROR_LOG_ID"),
                              Argument::ARGUMENT_VALUE("MISSING"));
    }

    // get error log id
    uint64_t errorId = 0;
    try
    {
        errorId = std::get<uint64_t>(iter->second);
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

    if (errorId > MAX_ERROR_LOG_ID)
    {
        // An error will be logged if the error log id is larger than maximum
        // value and set the error log id as 0.
        log<level::ERR>(fmt::format("Error log id is greater than maximum({}) "
                                    "length, setting as 0, errorid({})",
                                    MAX_ERROR_LOG_ID, errorId)
                            .c_str());
        report<InvalidArgument>(
            Argument::ARGUMENT_NAME("ERROR_LOG_ID"),
            Argument::ARGUMENT_VALUE(std::to_string(errorId).c_str()));
    }

    // Make it 8 char length string.
    std::stringstream ss;
    ss << std::setw(8) << std::setfill('0') << std::hex << errorId;
    std::string elogId = ss.str();

    uint8_t type = 0;
    uint64_t failingUnit = INVALID_FAILING_UNIT;

    if (dumpType == "com.ibm.Dump.Create.DumpType.Hostboot")
    {
        type = SBE::SBE_DUMP_TYPE_HOSTBOOT;
    }
    else if (dumpType == "com.ibm.Dump.Create.DumpType.Hardware")
    {
        type = SBE::SBE_DUMP_TYPE_HARDWARE;
    }
    else if (dumpType == "com.ibm.Dump.Create.DumpType.SBE")
    {
        type = SBE::SBE_DUMP_TYPE_SBE;
    }
    else
    {
        log<level::ERR>(
            fmt::format("Invalid dump type passed dumpType({})", dumpType)
                .c_str());
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("DUMP_TYPE"),
                              Argument::ARGUMENT_VALUE(dumpType.c_str()));
    }

    sdbusplus::message::object_path newDumpPath;
    if ((type == SBE::SBE_DUMP_TYPE_HARDWARE) ||
        (type == SBE::SBE_DUMP_TYPE_SBE))
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
            failingUnit = std::get<uint64_t>(iter->second);
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

        if (failingUnit > MAX_FAILING_UNIT)
        {
            log<level::ERR>(fmt::format("Invalid failing uint id: greater than "
                                        "maximum number({}): input({})",
                                        failingUnit, MAX_FAILING_UNIT)
                                .c_str());
            elog<InvalidArgument>(
                Argument::ARGUMENT_NAME("FAILING_UNIT_ID"),
                Argument::ARGUMENT_VALUE(std::to_string(failingUnit).c_str()));
        }
    }

    try
    {
        // Pass empty create parameters since no additional parameters
        // are needed.
        DumpCreateParams createDumpParams;
        auto dumpManager =
            util::getService(bus, DUMP_CREATE_IFACE, dumpInfo[type].dumpPath);

        auto method = bus.new_method_call(dumpManager.c_str(),
                                          dumpInfo[type].dumpPath.c_str(),
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
        elog<InternalFailure>();
    }

    // DUMP Path format /xyz/openbmc_project/dump/<dump_type>/entry/<id>
    std::string pathStr = newDumpPath;
    auto pos = pathStr.rfind("/");
    if (pos == std::string::npos)
    {
        log<level::ERR>(
            fmt::format("Invalid dump path, path({})", pathStr).c_str());
        elog<InternalFailure>();
    }

    auto idString = pathStr.substr(pos + 1);
    auto id = std::stoi(idString);

    pid_t pid = fork();
    if (pid == 0)
    {
        collectDump(type, id, elogId, failingUnit);
        std::exit(EXIT_SUCCESS);
    }
    else if (pid < 0)
    {
        // Fork failed
        log<level::ERR>("Failure in fork call");
        elog<InternalFailure>();
    }
    else
    {
        using namespace sdeventplus::source;
        Child::Callback callback =
            [this, type, id, pathStr](Child& eventSource, const siginfo_t* si) {
                eventSource.set_enabled(Enabled::On);
                if (si->si_status == 0)
                {
                    log<level::INFO>("Dump collected, initiating packaging");
                    auto dumpManager = util::getService(
                        bus, DUMP_NOTIFY_IFACE, dumpInfo[type].dumpPath);
                    auto method = bus.new_method_call(
                        dumpManager.c_str(), dumpInfo[type].dumpPath.c_str(),
                        DUMP_NOTIFY_IFACE, "Notify");
                    method.append(static_cast<uint32_t>(id),
                                  static_cast<uint64_t>(0));
                    bus.call_noreply(method);
                }
                else
                {
                    log<level::ERR>("Dump collection failed, updating status");
                    util::setProperty(DUMP_PROGRESS_IFACE, STATUS_PROP, pathStr,
                                      bus,
                                      "xyz.openbmc_project.Common.Progress."
                                      "OperationStatus.Failed");
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
    // return object path
    return newDumpPath;
}
} // namespace dump
} // namespace openpower

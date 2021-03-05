#include "config.h"

#include "dump_manager.hpp"

#include "dump_utils.hpp"

#include <ekb/chips/p10/procedures/hwp/sbe/p10_sbe_ext_defs.H>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/exception.hpp>
#include <system_error>
#include <vector>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include "attributes_info.H"

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

/* @struct DumpTypeInfo
 * @brief to store basic info about different dump types
 */
struct DumpTypeInfo
{
    std::string dumpPrefix;         // Prefix to dump files
    std::string dumpPath;           // D-Bus path of the dump
    std::string dumpCollectionPath; // Path were dumps are stored
};

/* Map of dump type to the basic info of the dumps */
std::map<uint8_t, DumpTypeInfo> dumpInfo = {
    {SBE::SBE_DUMP_TYPE_HOSTBOOT,
     {HB_DUMP_FILE_PREFIX, HB_DUMP_DBUS_PATH, HB_DUMP_COLLECTION_PATH}},
};

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
    if (type == 0)
    {
        return true;
    }
    return false;
}

void Manager::collectDumpFromSBE(struct pdbg_target* proc,
                                 std::filesystem::path& dumpPath,
                                 const uint64_t timestamp, const uint8_t type,
                                 const uint8_t clockState)
{
    using namespace sdbusplus::xyz::openbmc_project::Common::Error;
    namespace fileError = sdbusplus::xyz::openbmc_project::Common::File::Error;

    int error = 0;
    uint8_t* data = NULL;
    uint32_t len = 0;

    // TODO #ibm-openbmc/dev/3039
    // Check the SBE state before attempt the dump collection
    // Skip this SBE if the SBE is not in a good state.

    struct pdbg_target* pib = NULL;
    pdbg_for_each_target("pib", proc, pib)
    {
        if (pdbg_target_probe(pib) != PDBG_TARGET_ENABLED)
        {
            continue;
        }
        break;
    }

    if (pib == NULL)
    {
        throw std::system_error(error, std::generic_category(),
                                "No valid pib target found");
    }

    uint32_t chipPos = 0;
    if (DT_GET_PROP(ATTR_FAPI_POS, proc, chipPos))
    {
        log<level::ERR>("Attribute [ATTR_FAPI_POS] get failed");
        throw std::system_error(error, std::generic_category(),
                                "Attribute [ATTR_FAPI_POS] get failed");
    }

    // Filename format: <dump_type>_<time_stamp>_<chip_position>_<clock_state>
    std::string filename =
        dumpInfo[type].dumpPrefix + "_" + std::to_string(timestamp) + "_" +
        std::to_string(chipPos) + "_" + std::to_string(clockState);
    dumpPath /= filename;

    if ((error = sbe_dump(pib, type, clockState, &data, &len)) < 0)
    {
        // Free if the memory is allocated
        if (data != NULL)
        {
            free(data);
        }
        // Add a trace if the failure is on the secondary.
        if ((!isMasterProc(proc)) && (type == SBE::SBE_DUMP_TYPE_HOSTBOOT))
        {
            log<level::ERR>("Error in collecting dump from secondary, skipping",
                            entry("CHIPPOSITION=%d", chipPos));
            return;
        }
        log<level::ERR>("Failed to collect dump",
                        entry("CHIPPOSITION=%d", chipPos));
        // TODO Create a PEL in the future for this failure case.
        throw std::system_error(error, std::generic_category(),
                                "Failed to collect dump from SBE");
    }

    if ((len == 0) || (data == NULL))
    {
        // Add a trace if no data from secondary
        if ((!isMasterProc(proc)) && (type == SBE::SBE_DUMP_TYPE_HOSTBOOT))
        {
            log<level::INFO>(
                "No hostboot dump recieved from secondary SBE, skipping",
                entry("CHIPPOSITION=%d", chipPos));
            return;
        }
        log<level::ERR>("No data returned while collecting the dump",
                        entry("CHIPPOSITION=%d", chipPos));
        throw std::system_error(error, std::generic_category(),
                                "No data returned while collecting the dump");
    }

    std::ofstream outfile{dumpPath, std::ios::out | std::ios::binary};
    if (!outfile.good())
    {
        free(data);
        using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
        using metadata = xyz::openbmc_project::Common::File::Open;
        // Unable to open the file for writing
        auto err = errno;
        log<level::ERR>("Error opening file to write dump ",
                        entry("ERRNO=%d", err),
                        entry("FILEPATH=%s", dumpPath.c_str()));
        report<Open>(metadata::ERRNO(err), metadata::PATH(dumpPath.c_str()));
        // Just return here, so that the dumps collected from other
        // SBEs can be packaged.
        return;
    }

    outfile.exceptions(std::ifstream::failbit | std::ifstream::badbit |
                       std::ifstream::eofbit);

    try
    {
        outfile.write(reinterpret_cast<char*>(data), len);
        outfile.close();
        free(data);
    }
    catch (std::ofstream::failure& oe)
    {
        free(data);
        using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
        using metadata = xyz::openbmc_project::Common::File::Write;
        auto err = errno;
        log<level::ERR>("Failed to write to dump file ",
                        entry("ERR=%s", oe.what()), entry("ERRNO=%d", err),
                        entry("FILEPATH=%s", dumpPath.c_str()));
        report<Write>(metadata::ERRNO(err), metadata::PATH(dumpPath.c_str()));
        // Just return here so dumps collected from other SBEs can be
        // packaged.
        return;
    }
    log<level::INFO>("Hostboot dump collected");
}

void Manager::collectDump(uint8_t type, uint32_t id, std::string errorLogId)
{
    int error = 0;
    struct pdbg_target* target;
    std::vector<pid_t> pidList;
    bool failed = false;
    pdbg_targets_init(NULL);
    pdbg_set_loglevel(PDBG_INFO);
    auto timeNow = std::chrono::steady_clock::now();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                             timeNow.time_since_epoch())
                             .count();

    std::filesystem::path dumpPath(dumpInfo[type].dumpCollectionPath);
    dumpPath /= std::to_string(id);
    try
    {
        std::filesystem::create_directories(dumpPath);
    }
    catch (std::filesystem::filesystem_error& e)
    {
        log<level::ERR>("Error creating dump directories",
                        entry("FILEPATH=%s", dumpPath.c_str()));
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
        log<level::ERR>("Error opening file to write errorlog id ",
                        entry("ERRNO=%d", err),
                        entry("FILEPATH=%s", dumpPath.c_str()));
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
            auto err = errno;
            log<level::ERR>("Failed to write errorlog id to file ",
                            entry("ERR=%s", oe.what()), entry("ERRNO=%d", err),
                            entry("FILEPATH=%s", dumpPath.c_str()));
            report<Write>(metadata::ERRNO(err),
                          metadata::PATH(dumpPath.c_str()));
        }
    }

    std::vector<uint8_t> clockStates = {SBE::SBE_CLOCK_ON, SBE::SBE_CLOCK_OFF};
    for (auto cstate : clockStates)
    {
        pdbg_for_each_class_target("proc", target)
        {
            if (pdbg_target_probe(target) != PDBG_TARGET_ENABLED)
            {
                continue;
            }
            uint8_t hwasState[5];
            if (DT_GET_PROP(ATTR_HWAS_STATE, target, hwasState))
            {
                log<level::ERR>("Attribute [ATTR_HWAS_STATE] get failed");
                throw std::system_error(
                    error, std::generic_category(),
                    "Attribute [ATTR_HWAS_STATE] get failed");
            }
            // If the proc is not functional skip
            // isFuntional bit is stored in 4th byte and bit 3 position in
            // HWAS_STATE
            if (hwasState[4] & 0x20)
            {
                if (isMasterProc(target))
                {
                    // Primary processor is not functional
                    log<level::INFO>("Primary Processor is not functional");
                }
                continue;
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
                    collectDumpFromSBE(target, dumpPath, timestamp, type,
                                       cstate);
                }
                catch (const std::runtime_error& error)
                {
                    log<level::ERR>("Failed to execute collection",
                                    entry("ERR=%s", error.what()));
                    std::exit(EXIT_FAILURE);
                }
                std::exit(EXIT_SUCCESS);
            }
            else
            {
                pidList.push_back(std::move(pid));
            }
        }
    }

    for (auto& p : pidList)
    {
        int status = 0;
        waitpid(p, &status, 0);
        if (WEXITSTATUS(status))
        {
            log<level::ERR>("Dump collection failed");
            failed = true;
        }
        log<level::ERR>("Dump collection completed");
    }
    if (failed)
    {
        log<level::ERR>("Failed to collect the dump");
        std::exit(EXIT_FAILURE);
    }
}

sdbusplus::message::object_path
    Manager::createDump(std::map<std::string, std::string> params)
{
    using namespace phosphor::logging;
    std::vector<std::pair<std::string, std::string>> createDumpParams;
    sdbusplus::message::object_path newDumpPath;
    using InvalidArgument =
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
    using CreateParameters =
        sdbusplus::com::ibm::Dump::server::Create::CreateParameters;
    using Argument = xyz::openbmc_project::Common::InvalidArgument;

    std::string dumpType;
    std::string elogId;
    try
    {
        dumpType = params.at(
            sdbusplus::com::ibm::Dump::server::Create::
                convertCreateParametersToString(CreateParameters::DumpType));
    }
    catch (const std::out_of_range& e)
    {
        log<level::ERR>("Required argument, dump type is not passed",
                        entry("ERROR=%s", e.what()));
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("DUMP_TYPE"),
                              Argument::ARGUMENT_VALUE("MISSING"));
    }
    try
    {
        elogId = params.at(
            sdbusplus::com::ibm::Dump::server::Create::
                convertCreateParametersToString(CreateParameters::ErrorLogId));
    }
    catch (const std::out_of_range& e)
    {
        log<level::ERR>("Required argument, error log id is not passed",
                        entry("ERROR=%s", e.what()));
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("ERROR_LOG_ID"),
                              Argument::ARGUMENT_VALUE("MISSING"));
    }

    uint8_t type = 0;

    if (dumpType == "com.ibm.Dump.Create.DumpType.Hostboot")
    {
        type = SBE::SBE_DUMP_TYPE_HOSTBOOT;
    }
    else
    {
        log<level::ERR>("Invalid dump type passed");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("DUMP_TYPE"),
                              Argument::ARGUMENT_VALUE(dumpType.c_str()));
    }

    try
    {
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
        log<level::ERR>("D-Bus call exception", entry("ERROR=%s", e.what()));
        elog<InternalFailure>();
    }

    // DUMP Path format /xyz/openbmc_project/dump/hostboot/entry/<id>
    std::string pathStr = newDumpPath;
    auto pos = pathStr.rfind("/");
    if (pos == std::string::npos)
    {
        log<level::ERR>("Invalid dump path");
        elog<InternalFailure>();
    }

    auto idString = pathStr.substr(pos + 1);
    auto id = std::stoi(idString);

    pid_t pid = fork();
    if (pid == 0)
    {
        collectDump(type, id, elogId);
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
                    auto dumpManager = util::getService(
                        bus, DUMP_PROGRESS_IFACE, dumpInfo[type].dumpPath);
                    std::string failed = "xyz.openbmc_project.Common.Progress."
                                         "OperationStatus.Failed";
                    util::setProperty(DUMP_PROGRESS_IFACE, STATUS_PROP, pathStr,
                                      dumpManager, bus, failed);
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
    }
    // return object path
    return newDumpPath;
}
} // namespace dump
} // namespace openpower

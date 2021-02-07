#include "dump_manager.hpp"
#include "utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <system_error>
#include <vector>
#include <xyz/openbmc_project/Common/File/error.hpp>

#include "attributes_info.H"

namespace openpower
{
namespace dump
{

constexpr auto DUMP_CREATE_IFACE = "xyz.openbmc_project.Dump.Create";

/* @struct DumpTypeInfo
 * @brief to store basic info about different dump types
 */
struct DumpTypeInfo
{
    std::string dumpPrefix;
    std::string dumpPath;
};

std::map<uint8_t, DumpTypeInfo> dumpInfo = {
    {SBE_DUMP_TYPE_HOSTBOOT, {"hbdump_", "/xyz/openbmc_project/dump/hostboot"}},
};

bool Manager::isMasterProc(struct pdbg_target* proc)
{
    using namespace phosphor::logging;
    ATTR_PROC_MASTER_TYPE_Type type;
    // Get processor type (Master or Alt-master)
    if (!pdbg_target_get_attribute(proc, "ATTR_PROC_MASTER_TYPE", 1, 1, &type))
    {
        log<level::ERR>("Attribute [ATTR_PROC_MASTER_TYPE] get failed");
        throw std::runtime_error(
            "Attribute [ATTR_PROC_MASTER_TYPE] get failed");
    }

    // Attribute value 0 corresponds to master processor
    if (type == 0)
    {
        return true;
    }
    return false;
}

void Manager::collectDumpFromSBE(struct pdbg_target* tgt,
                                 std::filesystem::path& dumpPath,
                                 uint64_t timestamp, uint8_t type,
                                 uint8_t clockState)
{
    using namespace sdbusplus::xyz::openbmc_project::Common::Error;
    using namespace phosphor::logging;
    namespace fileError = sdbusplus::xyz::openbmc_project::Common::File::Error;

    int error = 0;
    uint8_t* data = NULL;
    uint32_t len = 0;
    struct pdbg_target* proc = pdbg_target_require_parent("proc", tgt);

    uint32_t chipPos = 0;
    pdbg_target_get_attribute(tgt, "ATTR_FAPI_POS", 4, 1, &chipPos);
    std::string filename =
        dumpInfo[type].dumpPrefix + "_" + std::to_string(timestamp) + "_" +
        std::to_string(chipPos) + "-" + std::to_string(clockState);
    dumpPath /= filename;

    if ((error = sbe_dump(tgt, type, clockState, &data, &len)) < 0)
    {
        if ((!isMasterProc(proc)) && (type == SBE_DUMP_TYPE_HOSTBOOT))
        {
            log<level::INFO>(
                "Failed to collect Hostboot dump from slave SBE, skipping",
                entry("CHIPPOSITION=%d", chipPos));
            return;
        }
        log<level::ERR>("Failed to collect dump",
                        entry("CHIPPOSITION=%d", chipPos));
        // TODO Create a PEL in the future for this failure case.
        throw std::system_error(error, std::generic_category(),
                                "Failed to initiate memory preserving reboot");
    }

    if (len == 0)
    {
        if ((!isMasterProc(proc)) && (type == SBE_DUMP_TYPE_HOSTBOOT))
        {
            log<level::INFO>(
                "No hostboot dump recieved from slave SBE, skipping",
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
        // Unable to open the file for writing
        auto err = errno;
        log<level::ERR>("Error opening file to write dump ",
                        entry("ERRNO=%d", err),
                        entry("FILEPATH=%s", dumpPath.c_str()));
        throw fileError::Open();
    }

    outfile.exceptions(std::ifstream::failbit | std::ifstream::badbit |
                       std::ifstream::eofbit);

    try
    {
        outfile.write(reinterpret_cast<char*>(data), len);
        outfile.close();
    }
    catch (std::ofstream::failure& oe)
    {
        auto err = errno;
        log<level::ERR>("Failed to write to dump file ",
                        entry("ERR=%s", oe.what()), entry("ERRNO=%d", err),
                        entry("FILEPATH=%s", dumpPath.c_str()));
        throw fileError::Write();
    }
    log<level::INFO>("Hostboot dump collected");
}

void Manager::collectDump(uint8_t type, uint32_t id, std::string errorLogId)
{
    using namespace sdbusplus::xyz::openbmc_project::Common::Error;
    using namespace phosphor::logging;
    namespace fileError = sdbusplus::xyz::openbmc_project::Common::File::Error;

    struct pdbg_target* target;
    std::vector<pid_t> pidList;
    bool failed = false;
    pdbg_targets_init(NULL);
    pdbg_set_loglevel(PDBG_DEBUG);
    auto timeNow = std::chrono::steady_clock::now();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                             timeNow.time_since_epoch())
                             .count();

    std::filesystem::path dumpPath(DUMP_TMP_PATH);
    dumpPath /= std::to_string(id);
    std::filesystem::create_directories(dumpPath);

    std::filesystem::path elogPath = dumpPath / "ErrorLog";
    std::ofstream outfile{elogPath, std::ios::out | std::ios::binary};
    if (!outfile.good())
    {
        // Unable to open the file for writing
        auto err = errno;
        log<level::ERR>("Error opening file to write errorlog id ",
                        entry("ERRNO=%d", err),
                        entry("FILEPATH=%s", dumpPath.c_str()));
        throw fileError::Open();
    }

    outfile.exceptions(std::ifstream::failbit | std::ifstream::badbit |
                       std::ifstream::eofbit);

    try
    {
        outfile << errorLogId;
        outfile.close();
    }
    catch (std::ofstream::failure& oe)
    {
        auto err = errno;
        log<level::ERR>("Failed to write errorlog id to file ",
                        entry("ERR=%s", oe.what()), entry("ERRNO=%d", err),
                        entry("FILEPATH=%s", dumpPath.c_str()));
        throw fileError::Write();
    }

    std::vector<uint8_t> clockStates = {SBE_CLOCK_ON, SBE_CLOCK_OFF};
    for (auto cstate : clockStates)
    {
        pdbg_for_each_class_target("pib", target)
        {
            if (pdbg_target_probe(target) != PDBG_TARGET_ENABLED)
            {
                continue;
            }

            pid_t pid = fork();

            if (pid < 0)
            {
                log<level::ERR>(
                    "Fork failed while starting hostboot dump collection");
                failed = true;
            }
            else if (pid == 0)
            {
                collectDumpFromSBE(target, dumpPath, timestamp, type, cstate);
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
        throw std::runtime_error("Failed to collect the dump");
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

    std::string dumpType =
        params[sdbusplus::com::ibm::Dump::server::Create::
                   convertCreateParametersToString(CreateParameters::DumpType)];
    std::string elogId = params[sdbusplus::com::ibm::Dump::server::Create::
                                    convertCreateParametersToString(
                                        CreateParameters::ErrorLogId)];
    uint8_t type = 0;

    if (dumpType == "com.ibm.Dump.Create.DumpType.Hostboot")
    {
        type = SBE_DUMP_TYPE_HOSTBOOT;
    }
    else
    {
        throw InvalidArgument();
    }

    try
    {
        std::variant<sdbusplus::message::object_path> objPathResponse;
        auto dumpManager =
            getService(bus, DUMP_CREATE_IFACE, dumpInfo[type].dumpPath);

        auto method = bus.new_method_call(dumpManager.c_str(),
                                          dumpInfo[type].dumpPath.c_str(),
                                          DUMP_CREATE_IFACE, "CreateDump");
        method.append(createDumpParams);
        auto response = bus.call(method);
        response.read(objPathResponse);
        newDumpPath =
            std::get<sdbusplus::message::object_path>(objPathResponse);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("D-Bus call exception");
        throw std::runtime_error("Failed to initiate dump");
    }
    catch (const std::bad_variant_access& e)
    {
        log<level::ERR>("Bad object path");
        throw std::runtime_error("Failed to get object path");
    }

    std::string pathStr = newDumpPath;
    auto pos = pathStr.rfind("/");
    if (pos == std::string::npos)
    {
        throw std::runtime_error("Invalid object path");
    }

    auto idString = pathStr.substr(pos + 1);
    auto id = std::stoi(idString);
    collectDump(type, id, elogId);

    // return object path
    return newDumpPath;
}
} // namespace dump
} // namespace openpower

extern "C"
{
#include <libpdbg_sbe.h>
}

#include "config.h"

#include "attributes_info.H"

#include "dump_collect.hpp"
#include "dump_utils.hpp"
#include "sbe_consts.hpp"

#include <fmt/core.h>
#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <chrono>
#include <fstream>
#include <string>
#include <system_error>

namespace openpower
{
namespace dump
{
using namespace phosphor::logging;
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

namespace sbe_chipop
{

void collectDumpFromSBE(struct pdbg_target* proc,
                        std::filesystem::path& dumpPath, const uint32_t id,
                        const uint8_t type, const uint8_t clockState,
                        const uint8_t chipPos, const uint8_t collectFastArray)
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
    util::DumpDataPtr dataPtr;
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
        if ((!util::isMasterProc(proc)) &&
            (type == SBE::SBE_DUMP_TYPE_HOSTBOOT))
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
        if ((!util::isMasterProc(proc)) &&
            (type == SBE::SBE_DUMP_TYPE_HOSTBOOT))
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

void collectDump(uint8_t type, uint32_t id, const uint64_t failingUnit,
                 std::filesystem::path& sbeFilePath)
{
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
                if (util::isMasterProc(target))
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
} // namespace sbe_chipop

void prepareCollection(const std::filesystem::path dumpPath,
                       std::string collectionPath, std::string errorLogId)
{
    std::filesystem::path sbeFilePath = dumpPath / collectionPath;
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
}

} // namespace dump
} // namespace openpower

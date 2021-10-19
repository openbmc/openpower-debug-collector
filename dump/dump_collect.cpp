extern "C"
{
#include <libpdbg_sbe.h>
}

#include "create_pel.hpp"
#include "dump_collect.hpp"

#include <fmt/core.h>
#include <libphal.H>
#include <phal_exception.H>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sbe_consts.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>

namespace openpower
{
namespace dump
{
namespace sbe_chipop
{

void writeDumpFile(const std::filesystem::path& path, const uint32_t id,
                   const uint8_t clockState, const uint8_t chipPos,
                   util::DumpDataPtr& dataPtr, const uint32_t len)
{
    using namespace phosphor::logging;
    using namespace sdbusplus::xyz::openbmc_project::Common::Error;
    namespace fileError = sdbusplus::xyz::openbmc_project::Common::File::Error;

    // Filename format: <dump_id>.SbeDataClocks<On/Off>.node0.proc<number>
    std::stringstream ss;
    ss << std::setw(8) << std::setfill('0') << id;
    std::string clockStr = (clockState == SBE::SBE_CLOCK_ON) ? "On" : "Off";

    // Assuming only node0 is supported now
    auto filename = ss.str() + ".SbeDataClocks" + clockStr + ".node0.proc" +
                    std::to_string(chipPos);

    std::filesystem::path dumpPath = path / filename;
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
    }
    outfile.close();
}

void collectDumpFromSBE(struct pdbg_target* proc,
                        const std::filesystem::path& path, const uint32_t id,
                        const uint8_t type, const uint8_t clockState,
                        const uint64_t failingUnit)
{
    using namespace phosphor::logging;
    auto chipPos = pdbg_target_index(proc);
    log<level::INFO>(fmt::format("Collect dump from proc({}) path({}) id({}) "
                                 "type({}) clock({}) failingUnit({})",
                                 chipPos, path.string(), id, type, clockState,
                                 failingUnit)
                         .c_str());

    util::DumpDataPtr dataPtr;
    uint32_t len = 0;
    uint8_t collectFastArray = 0;

    try
    {
        openpower::phal::sbe::getDump(proc, type, clockState, collectFastArray,
                                      dataPtr.getPtr(), &len);
    }
    catch (const openpower::phal::sbeError_t& sbeError)
    {
        if (sbeError.errType() ==
            openpower::phal::exception::SBE_CHIPOP_NOT_ALLOWED)
        {
            // SBE is not ready to accept chip-ops,
            // Skip the request, no additional error handling required.
            log<level::INFO>(fmt::format("Collect dump: Skipping ({}) dump({}) "
                                         "on proc({}) clock state({})",
                                         sbeError.what(), type, chipPos,
                                         clockState)
                                 .c_str());
            return;
        }
        log<level::ERR>(
            fmt::format(
                "Error in collecting dump dump type({}), clockstate({}), proc "
                "position({}), collectFastArray({}) error({})",
                type, clockState, chipPos, collectFastArray, sbeError.what())
                .c_str());

        std::string event = "org.open_power.Processor.Error.SbeChipOpFailure";

        openpower::dump::pel::FFDCData pelAdditionalData;
        uint32_t cmd = SBE::SBEFIFO_CMD_CLASS_DUMP | SBE::SBEFIFO_CMD_GET_DUMP;

        pelAdditionalData.emplace_back("SRC6",
                                       std::to_string((chipPos << 16) | cmd));

        openpower::dump::pel::createSbeErrorPEL(event, sbeError,
                                                pelAdditionalData);
        return;
    }
    writeDumpFile(path, id, clockState, chipPos, dataPtr, len);
}

void collectDump(const uint8_t type, const uint32_t id,
                 const uint64_t failingUnit, const std::filesystem::path& path)
{
    using namespace phosphor::logging;
    log<level::INFO>(
        fmt::format(
            "Dump collection started type({}) id({}) failingUnit({}), path({})",
            type, id, failingUnit, path.string())
            .c_str());
    struct pdbg_target* target = nullptr;
    auto failed = false;
    // Initialize PDBG
    openpower::phal::pdbg::init();
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
            if (!openpower::phal::pdbg::isTgtFunctional(target))
            {
                if (openpower::phal::pdbg::isPrimaryProc(target))
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
                throw std::runtime_error(
                    "Fork failed while starting hostboot dump collection");
            }
            else if (pid == 0)
            {
                try
                {
                    collectDumpFromSBE(target, path, id, type, cstate,
                                       failingUnit);
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
            auto status = 0;
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
} // namespace dump
} // namespace openpower

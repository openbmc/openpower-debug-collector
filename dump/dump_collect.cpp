extern "C"
{
#include <libpdbg.h>
#include <libpdbg_sbe.h>
}

#include "dump_collect.hpp"
#include "sbe_consts.hpp"

#include <libphal.H>
#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sbe_consts.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>

namespace openpower
{
namespace dump
{
namespace sbe_chipop
{
using namespace phosphor::logging;
using namespace openpower::dump::SBE;

void SbeDumpCollector::collectDump(uint8_t type, uint32_t id,
                                   uint64_t failingUnit,
                                   const std::filesystem::path& path)
{
    log<level::INFO>(
        std::format(
            "Starting dump collection: type({}) id({}) failingUnit({}), path({})",
            type, id, failingUnit, path.string())
            .c_str());

    initializePdbg();

    std::vector<struct pdbg_target*> targets;

    struct pdbg_target* target = nullptr;
    pdbg_for_each_class_target("proc", target)
    {
        if (pdbg_target_probe(target) != PDBG_TARGET_ENABLED ||
            !openpower::phal::pdbg::isTgtFunctional(target))
        {
            continue;
        }

        targets.push_back(target);
    }

    std::vector<uint8_t> clockStates = {SBE_CLOCK_ON, SBE_CLOCK_OFF};
    for (auto cstate : clockStates)
    {
        auto futures = spawnDumpCollectionProcesses(type, id, path, failingUnit,
                                                    cstate, targets);

        // Wait for all asynchronous tasks to complete
        for (auto& future : futures)
        {
            try
            {
                future.wait();
            }
            catch (const std::exception& e)
            {
                log<level::ERR>(
                    std::format("Failed to collect dump from SBE ErrorMsg({})",
                                e.what())
                        .c_str());
            }
        }
        log<level::INFO>(
            std::format(
                "Dump collection completed for clock state({}): type({}) id({}) failingUnit({}), path({})",
                cstate, type, id, failingUnit, path.string())
                .c_str());
    }
    if (std::filesystem::is_empty(path))
    {
        log<level::ERR>("Failed to collect the dump");
        throw std::runtime_error("Failed to collect the dump");
    }
    log<level::INFO>("Dump collection completed");
}

void SbeDumpCollector::initializePdbg()
{
    openpower::phal::pdbg::init();
}

std::vector<std::future<void>> SbeDumpCollector::spawnDumpCollectionProcesses(
    uint8_t type, uint32_t id, const std::filesystem::path& path,
    uint64_t failingUnit, uint8_t cstate,
    const std::vector<struct pdbg_target*>& targets)
{
    std::vector<std::future<void>> futures;

    for (auto target : targets)
    {
        if (pdbg_target_probe(target) != PDBG_TARGET_ENABLED ||
            !openpower::phal::pdbg::isTgtFunctional(target))
        {
            continue;
        }

        auto future =
            std::async(std::launch::async,
                       [this, target, path, id, type, cstate, failingUnit]() {
            try
            {
                this->collectDumpFromSBE(target, path, id, type, cstate,
                                         failingUnit);
            }
            catch (const std::exception& e)
            {
                log<level::ERR>(
                    std::format("Failed to collect dump from SBE on Proc-({})",
                                pdbg_target_index(target))
                        .c_str());
            }
        });

        futures.push_back(std::move(future));
    }

    return futures;
}

void SbeDumpCollector::collectDumpFromSBE(struct pdbg_target* proc,
                                          const std::filesystem::path& path,
                                          uint32_t id, uint8_t type,
                                          uint8_t clockState,
                                          uint64_t failingUnit)
{
    auto chipPos = pdbg_target_index(proc);
    log<level::INFO>(
        std::format(
            "Collecting dump from proc({}): path({}) id({}) type({}) clockState({}) failingUnit({})",
            chipPos, path.string(), id, type, clockState, failingUnit)
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
            log<level::INFO>(std::format("Collect dump: Skipping ({}) dump({}) "
                                         "on proc({}) clock state({})",
                                         sbeError.what(), type, chipPos,
                                         clockState)
                                 .c_str());
            return;
        }
        log<level::ERR>(
            std::format(
                "Error in collecting dump dump type({}), clockstate({}), proc "
                "position({}), collectFastArray({}) error({})",
                type, clockState, chipPos, collectFastArray, sbeError.what())
                .c_str());
        return;
    }
    writeDumpFile(path, id, clockState, chipPos, dataPtr, len);
}

void SbeDumpCollector::writeDumpFile(const std::filesystem::path& path,
                                     const uint32_t id,
                                     const uint8_t clockState,
                                     const uint8_t chipPos,
                                     util::DumpDataPtr& dataPtr,
                                     const uint32_t len)
{
    using namespace sdbusplus::xyz::openbmc_project::Common::Error;
    namespace fileError = sdbusplus::xyz::openbmc_project::Common::File::Error;

    // Construct the filename
    std::ostringstream filenameBuilder;
    filenameBuilder << std::setw(8) << std::setfill('0') << id
                    << ".SbeDataClocks"
                    << (clockState == SBE_CLOCK_ON ? "On" : "Off")
                    << ".node0.proc" << static_cast<int>(chipPos);

    auto dumpPath = path / filenameBuilder.str();

    // Attempt to open the file
    std::ofstream outfile(dumpPath, std::ios::out | std::ios::binary);
    if (!outfile)
    {
        using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
        using metadata = xyz::openbmc_project::Common::File::Open;
        // Unable to open the file for writing
        auto err = errno;
        log<level::ERR>(
            std::format(
                "Error opening file to write dump, errno({}), filepath({})",
                err, dumpPath.string())
                .c_str());
        report<Open>(metadata::ERRNO(err), metadata::PATH(dumpPath.c_str()));
        // Just return here, so that the dumps collected from other
        // SBEs can be packaged.
        return;
    }

    // Write to the file
    try
    {
        outfile.write(reinterpret_cast<const char*>(dataPtr.getData()), len);
        log<level::INFO>("Successfully wrote dump file",
                         entry("PATH=%s", dumpPath.c_str()),
                         entry("SIZE=%u", len));
    }
    catch (const std::ofstream::failure& oe)
    {
        using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
        using metadata = xyz::openbmc_project::Common::File::Write;
        log<level::ERR>(std::format("Failed to write to dump file, "
                                    "errorMsg({}), error({}), filepath({})",
                                    oe.what(), oe.code().value(),
                                    dumpPath.string())
                            .c_str());
        report<Write>(metadata::ERRNO(oe.code().value()),
                      metadata::PATH(dumpPath.c_str()));
        // Just return here so dumps collected from other SBEs can be
        // packaged.
    }
}

} // namespace sbe_chipop
} // namespace dump
} // namespace openpower

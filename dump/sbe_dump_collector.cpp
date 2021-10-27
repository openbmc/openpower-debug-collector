extern "C"
{
#include <libpdbg.h>
#include <libpdbg_sbe.h>
}

#include "sbe_consts.hpp"
#include "sbe_dump_collector.hpp"

#include <libphal.H>
#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>
#include <phosphor-logging/log.hpp>
#include <sbe_consts.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>

namespace openpower::dump::sbe_chipop
{

using namespace phosphor::logging;
using namespace openpower::dump::SBE;

void SbeDumpCollector::collectDump(uint8_t type, uint32_t id,
                                   uint64_t failingUnit,
                                   const std::filesystem::path& path)
{
    lg2::error("Starting dump collection: type:{TYPE} id:{ID} "
               "failingUnit:{FAILINGUNIT}, path:{PATH}",
               "TYPE", type, "ID", id, "FAILINGUNIT", failingUnit, "PATH",
               path.string());

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
                lg2::error("Failed to collect dump from SBE ErrorMsg({ERROR})",
                           "ERROR", e);
            }
        }
        lg2::info(
            "Dump collection completed for clock state({CSTATE}): type({TYPE}) "
            "id({ID}) failingUnit({FAILINGUNIT}), path({PATH})",
            "CSTATE", cstate, "TYPE", type, "ID", id, "FAILINGUNIT",
            failingUnit, "PATH", path.string());
    }
    if (std::filesystem::is_empty(path))
    {
        lg2::error("Failed to collect the dump");
        throw std::runtime_error("Failed to collect the dump");
    }
    lg2::info("Dump collection completed");
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
                lg2::error(
                    "Failed to collect dump from SBE on Proc-({PROCINDEX})",
                    "PROCINDEX", pdbg_target_index(target));
            }
        });

        futures.push_back(std::move(future));
    }

    return futures;
}

void SbeDumpCollector::collectDumpFromSBE(struct pdbg_target* chip,
                                          const std::filesystem::path& path,
                                          uint32_t id, uint8_t type,
                                          uint8_t clockState,
                                          uint64_t failingUnit)
{
    auto chipPos = pdbg_target_index(chip);
    lg2::info(
        "Collecting dump from proc({PROC}): path({PATH}) id({ID}) "
        "type({TYPE}) clockState({CLOCKSTATE}) failingUnit({FAILINGUNIT})",
        "PROC", chipPos, "PATH", path.string(), "ID", id, "TYPE", type,
        "CLOCKSTATE", clockState, "FAILINGUNIT", failingUnit);

    util::DumpDataPtr dataPtr;
    uint32_t len = 0;
    uint8_t collectFastArray =
        checkFastarrayCollectionNeeded(clockState, type, failingUnit, chipPos);

    try
    {
        openpower::phal::sbe::getDump(chip, type, clockState, collectFastArray,
                                      dataPtr.getPtr(), &len);
    }
    catch (const openpower::phal::sbeError_t& sbeError)
    {
        if (sbeError.errType() ==
            openpower::phal::exception::SBE_CHIPOP_NOT_ALLOWED)
        {
            // SBE is not ready to accept chip-ops,
            // Skip the request, no additional error handling required.
            lg2::info("Collect dump: Skipping ({ERROR}) dump({TYPE}) "
                      "on proc({PROC}) clock state({CLOCKSTATE})",
                      "ERROR", sbeError, "TYPE", type, "PROC", chipPos,
                      "CLOCKSTATE", clockState);
            return;
        }

        lg2::error("Error in collecting dump dump type({TYPE}), "
                   "clockstate({CLOCKSTATE}), proc position({PROC}), "
                   "collectFastArray({COLLECTFASTARRAY}) error({ERROR})",
                   "TYPE", type, "CLOCKSTATE", clockState, "PROC", chipPos,
                   "COLLECTFASTARRAY", collectFastArray, "ERROR", sbeError);

        return;
    }
    writeDumpFile(path, id, clockState, 0, "proc", chipPos, dataPtr, len);
}

void SbeDumpCollector::writeDumpFile(
    const std::filesystem::path& path, const uint32_t id,
    const uint8_t clockState, const uint8_t nodeNum, std::string chipName,
    const uint8_t chipPos, util::DumpDataPtr& dataPtr, const uint32_t len)
{
    using namespace sdbusplus::xyz::openbmc_project::Common::Error;
    namespace fileError = sdbusplus::xyz::openbmc_project::Common::File::Error;

    // Construct the filename
    std::ostringstream filenameBuilder;
    filenameBuilder << std::setw(8) << std::setfill('0') << id
                    << ".SbeDataClocks"
                    << (clockState == SBE_CLOCK_ON ? "On" : "Off") << ".node"
                    << static_cast<int>(nodeNum) << "." << chipName
                    << static_cast<int>(chipPos);

    auto dumpPath = path / filenameBuilder.str();

    // Attempt to open the file
    std::ofstream outfile(dumpPath, std::ios::out | std::ios::binary);
    if (!outfile)
    {
        using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
        using metadata = xyz::openbmc_project::Common::File::Open;
        // Unable to open the file for writing
        auto err = errno;
        lg2::error("Error opening file to write dump, "
                   "errno({ERRNO}), filepath({FILEPATH})",
                   "ERRNO", err, "FILEPATH", dumpPath.string());

        report<Open>(metadata::ERRNO(err), metadata::PATH(dumpPath.c_str()));
        // Just return here, so that the dumps collected from other
        // SBEs can be packaged.
        return;
    }

    // Write to the file
    try
    {
        outfile.write(reinterpret_cast<const char*>(dataPtr.getData()), len);

        lg2::info("Successfully wrote dump file "
                  "path=({PATH}) size=({SIZE})",
                  "PATH", dumpPath.string(), "SIZE", len);
    }
    catch (const std::ofstream::failure& oe)
    {
        using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
        using metadata = xyz::openbmc_project::Common::File::Write;

        lg2::error(
            "Failed to write to dump file, "
            "errorMsg({ERROR}), error({ERRORCODE}), filepath({FILEPATH})",
            "ERROR", oe, "ERRORCODE", oe.code().value(), "FILEPATH",
            dumpPath.string());
        report<Write>(metadata::ERRNO(oe.code().value()),
                      metadata::PATH(dumpPath.c_str()));
        // Just return here so dumps collected from other SBEs can be
        // packaged.
    }
}

} // namespace openpower::dump::sbe_chipop

extern "C"
{
#include <libpdbg.h>
#include <libpdbg_sbe.h>
}

#include "create_pel.hpp"
#include "dump_collect.hpp"
#include "sbe_consts.hpp"
#include "sbe_type.hpp"

#include <libphal.H>
#include <phal_exception.H>

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
    log<level::INFO>(std::format("Starting dump collection: "
                                 "type({}) id({}) failingUnit({}), path({})",
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

        bool includeTarget = true;
        // if the dump type is hostboot then call stop instructions
        if (type == SBE_DUMP_TYPE_HOSTBOOT)
        {
            includeTarget = executeThreadStop(target);
        }
        if (includeTarget)
        {
            targets.push_back(target);
        }
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
            std::format("Dump collection completed for clock state({}):"
                        "type({}) id({}) failingUnit({}), path({})",
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

void SbeDumpCollector::logErrorAndCreatePEL(
    const openpower::phal::sbeError_t& sbeError, uint64_t chipPos,
    SBETypes sbeType, uint32_t cmdClass, uint32_t cmdType)
{
    std::string event = sbeTypeAttributes[sbeType].chipOpFailure;
    auto dumpIsRequired = false;

    // Determine the event and whether a dump is required based on sbeError type
    if (sbeError.errType() == openpower::phal::exception::SBE_CMD_TIMEOUT)
    {
        event = sbeTypeAttributes[sbeType].chipOpTimeout;
        dumpIsRequired = true;
    }

    // Log the error with relevant details
    log<level::ERR>(
        std::format("SBE operation error, chip position({}), error({}), "
                    "command class({:X}), command type({:X}), event({})",
                    chipPos, sbeError.what(), cmdClass, cmdType, event)
            .c_str());

    // Prepare additional data for PEL creation
    openpower::dump::pel::FFDCData pelAdditionalData = {
        {"SRC6", std::format("{:X}{:X}", chipPos, (cmdClass | cmdType))}};

    // Create PEL
    auto logId = openpower::dump::pel::createSbeErrorPEL(event, sbeError,
                                                         pelAdditionalData);

    // Request SBE Dump if required
    if (dumpIsRequired)
    {
        try
        {
            util::requestSBEDump(chipPos, logId, sbeType);
        }
        catch (const std::exception& e)
        {
            log<level::ERR>(
                std::format(
                    "SBE Dump request failed, chip position({}), Error: {}",
                    chipPos, e.what())
                    .c_str());
        }
    }
}

void SbeDumpCollector::collectDumpFromSBE(struct pdbg_target* chip,
                                          const std::filesystem::path& path,
                                          uint32_t id, uint8_t type,
                                          uint8_t clockState,
                                          uint64_t failingUnit)
{
    auto chipPos = pdbg_target_index(chip);
    SBETypes sbeType = getSBEType(chip);
    log<level::INFO>(
        std::format("Collecting dump from ({})({}): path({}) id({}) "
                    "type({}) clockState({}) failingUnit({})",
                    sbeTypeAttributes[sbeType].chipName, chipPos, path.string(),
                    id, type, clockState, failingUnit)
            .c_str());

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
            log<level::INFO>(
                std::format("Collect dump id({}): Skipping ({}) dump({}) "
                            "on ({}) ({}) clock state({})",
                            id, sbeError.what(), type,
                            sbeTypeAttributes[sbeType].chipName, chipPos,
                            clockState)
                    .c_str());
            return;
        }
        log<level::ERR>(
            std::format(
                "Error in collecting dump id({}) type({}), clockstate({}), ({}) "
                "position({}), error({})",
                id, type, clockState, sbeTypeAttributes[sbeType].chipName,
                chipPos, sbeError.what())
                .c_str());
        logErrorAndCreatePEL(sbeError, chipPos, sbeType, SBEFIFO_CMD_CLASS_DUMP,
                             SBEFIFO_CMD_GET_DUMP);
        return;
    }
    writeDumpFile(path, id, clockState, 0, sbeTypeAttributes[sbeType].chipName,
                  chipPos, dataPtr, len);
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
        log<level::INFO>(std::format("Successfully wrote dump file "
                                     "path=({}) size=({})",
                                     dumpPath.string(), len)
                             .c_str());
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

uint8_t SbeDumpCollector::checkFastarrayCollectionNeeded(
    const uint8_t clockState, const uint8_t type, uint64_t failingUnit,
    const uint8_t chipPos)
{
    return (clockState == SBE_CLOCK_OFF &&
            (type == SBE_DUMP_TYPE_HOSTBOOT ||
             (type == SBE_DUMP_TYPE_HARDWARE && chipPos == failingUnit)))
               ? 1
               : 0;
}

SBETypes SbeDumpCollector::getSBEType(struct pdbg_target* chip)
{
    return SBETypes::PROC;
}

bool SbeDumpCollector::executeThreadStop(struct pdbg_target* target)
{
    try
    {
        openpower::phal::sbe::threadStopProc(target);
        return true;
    }
    catch (const openpower::phal::sbeError_t& sbeError)
    {
        uint64_t chipPos = pdbg_target_index(target);
        if (sbeError.errType() ==
            openpower::phal::exception::SBE_CHIPOP_NOT_ALLOWED)
        {
            log<level::INFO>(std::format("SBE is not ready to accept chip-op: "
                                         "Skipping proc-({}) error({}) ",
                                         chipPos, sbeError.what())
                                 .c_str());
            return false; // Do not include the target for dump collection
        }
        logErrorAndCreatePEL(sbeError, chipPos, SBEFIFO_CMD_CLASS_INSTRUCTION,
                             SBEFIFO_CMD_CONTROL_INSN);
        // For TIMEOUT, log the error and skip adding the processor for dump
        // collection
        if (sbeError.errType() == openpower::phal::exception::SBE_CMD_TIMEOUT)
        {
            return false;
        }
    }
    // Include the target for dump collection for SBE_CMD_FAILED or any other
    // non-critical errors
    return true;
}

} // namespace sbe_chipop
} // namespace dump
} // namespace openpower

extern "C"
{
#include <libpdbg.h>
#include <libpdbg_sbe.h>
}

#include "create_pel.hpp"
#include "sbe_consts.hpp"
#include "sbe_dump_collector.hpp"
#include "sbe_type.hpp"

#include <libphal.H>
#include <phal_exception.H>

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
        // Skip collection for performance dump if clock state is not ON
        if (type == SBE_DUMP_TYPE_PERFORMANCE && cstate != SBE_CLOCK_ON)
        {
            continue;
        }
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

void SbeDumpCollector::logErrorAndCreatePEL(
    const openpower::phal::sbeError_t& sbeError, uint64_t chipPos,
    SBETypes sbeType, uint32_t cmdClass, uint32_t cmdType)
{
    try
    {
        std::string event = sbeTypeAttributes.at(sbeType).chipOpFailure;
        auto dumpIsRequired = false;

        if (sbeError.errType() == openpower::phal::exception::SBE_CMD_TIMEOUT)
        {
            event = sbeTypeAttributes.at(sbeType).chipOpTimeout;
            dumpIsRequired = true;
        }

        openpower::dump::pel::FFDCData pelAdditionalData = {
            {"SRC6", std::format("{:X}{:X}", chipPos, (cmdClass | cmdType))}};

        openpower::dump::pel::createSbeErrorPEL(event, sbeError,
                                                pelAdditionalData);
        auto logId = openpower::dump::pel::createSbeErrorPEL(event, sbeError,
                                                             pelAdditionalData);

        // Request SBE Dump if required
        if (dumpIsRequired)
        {
            util::requestSBEDump(chipPos, logId, sbeType);
        }
    }
    catch (const std::out_of_range& e)
    {
        lg2::error("Unknown SBE Type({SBETYPE}) ErrorMsg({ERROR})", "SBETYPE",
                   sbeType, "ERROR", e);
    }
    catch (const std::exception& e)
    {
        lg2::error("SBE Dump request failed, chip position({CHIPPOS}), "
                   "Error: {ERROR}",
                   "CHIPPOS", chipPos, "ERROR", e);
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
    auto chipName = sbeTypeAttributes.at(sbeType).chipName;
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
                   "clockstate({CLOCKSTATE}), chip type({CHIPTYPE}) "
                   "position({POSITION}), "
                   "collectFastArray({COLLECTFASTARRAY}) error({ERROR})",
                   "TYPE", type, "CLOCKSTATE", clockState, "CHIPTYPE", chipName,
                   "POSITION", chipPos, "COLLECTFASTARRAY", collectFastArray,
                   "ERROR", sbeError);
        logErrorAndCreatePEL(sbeError, chipPos, sbeType, SBEFIFO_CMD_CLASS_DUMP,
                             SBEFIFO_CMD_GET_DUMP);
        return;
    }
    writeDumpFile(path, id, clockState, 0, chipName, chipPos, dataPtr, len);
}

void SbeDumpCollector::writeDumpFile(
    const std::filesystem::path& path, const uint32_t id,
    const uint8_t clockState, const uint8_t nodeNum,
    const std::string& chipName, const uint8_t chipPos,
    util::DumpDataPtr& dataPtr, const uint32_t len)
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
            lg2::info("SBE is not ready to accept chip-op: Skipping "
                      "stop instruction on proc-({POSITION}) error({ERROR}) ",
                      "POSITION", chipPos, "ERROR", sbeError);
            return false; // Do not include the target for dump collection
        }

        lg2::error("Stop instructions failed on "
                   "proc-({POSITION}) error({ERROR}) ",
                   "POSITION", chipPos, "ERROR", sbeError);

        logErrorAndCreatePEL(sbeError, chipPos, SBETypes::PROC,
                             SBEFIFO_CMD_CLASS_INSTRUCTION,
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

} // namespace openpower::dump::sbe_chipop

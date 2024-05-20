extern "C"
{
#include <libpdbg.h>
#include <libpdbg_sbe.h>
}

#include "create_pel.hpp"
#include "sbe_consts.hpp"
#include "sbe_dump_collector.hpp"
#include "sbe_type.hpp"

#include <ekb/hwpf/fapi2/include/target_types.H>
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
using Severity = sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;

void SbeDumpCollector::collectDump(uint8_t type, uint32_t id,
                                   uint64_t failingUnit,
                                   const std::filesystem::path& path)
{
    lg2::error("Starting dump collection: type:{TYPE} id:{ID} "
               "failingUnit:{FAILINGUNIT}, path:{PATH}",
               "TYPE", type, "ID", id, "FAILINGUNIT", failingUnit, "PATH",
               path.string());

    initializePdbg();

    TargetMap targets;

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
            targets[target] = std::vector<struct pdbg_target*>();

            // Hardware dump needs OCMB data if present
            if (type == openpower::dump::SBE::SBE_DUMP_TYPE_HARDWARE)
            {
                struct pdbg_target* ocmbTarget;
                pdbg_for_each_target("ocmb", target, ocmbTarget)
                {
                    if (!is_ody_ocmb_chip(ocmbTarget))
                    {
                        continue;
                    }

                    if (pdbg_target_probe(ocmbTarget) != PDBG_TARGET_ENABLED)
                    {
                        continue;
                    }

                    if (!openpower::phal::pdbg::isTgtFunctional(ocmbTarget))
                    {
                        continue;
                    }
                    targets[target].push_back(ocmbTarget);
                }
            }
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
    uint64_t failingUnit, uint8_t cstate, const TargetMap& targetMap)
{
    std::vector<std::future<void>> futures;

    for (const auto& [procTarget, ocmbTargets] : targetMap)
    {
        auto future = std::async(std::launch::async,
                                 [this, procTarget, ocmbTargets, path, id, type,
                                  cstate, failingUnit]() {
            try
            {
                this->collectDumpFromSBE(procTarget, path, id, type, cstate,
                                         failingUnit);
            }
            catch (const std::exception& e)
            {
                lg2::error(
                    "Failed to collect dump from SBE on Proc-({PROCINDEX}) {ERROR}",
                    "PROCINDEX", pdbg_target_index(procTarget), "ERROR", e);
            }

            // Collect OCMBs only with clock on
            if (cstate == SBE_CLOCK_ON)
            {
                // Handle OCMBs serially after handling the proc
                for (auto ocmbTarget : ocmbTargets)
                {
                    try
                    {
                        this->collectDumpFromSBE(ocmbTarget, path, id, type,
                                                 cstate, failingUnit);
                    }
                    catch (const std::exception& e)
                    {
                        lg2::error(
                            "Failed to collect dump from OCMB -({OCMBINDEX}) {ERROR}",
                            "OCMBINDEX", pdbg_target_index(ocmbTarget), "ERROR",
                            e);
                    }
                }
            }
        });

        futures.push_back(std::move(future));
    }

    return futures;
}

bool SbeDumpCollector::logErrorAndCreatePEL(
    const openpower::phal::sbeError_t& sbeError, uint64_t chipPos,
    SBETypes sbeType, uint32_t cmdClass, uint32_t cmdType)
{
    namespace fs = std::filesystem;

    std::string chipName;
    std::string event;
    bool dumpIsRequired = false;
    bool isDumpFailure = true;
    try
    {
        chipName = sbeTypeAttributes.at(sbeType).chipName;
        event = sbeTypeAttributes.at(sbeType).chipOpFailure;

        lg2::info("log error {CHIP} {POSITION}", "CHIP", chipName, "POSITION",
                  chipPos);

        // Common FFDC data
        openpower::dump::pel::FFDCData pelAdditionalData = {
            {"SRC6", std::format("0x{:X}{:X}", chipPos, (cmdClass | cmdType))}};

        if (sbeType == SBETypes::OCMB)
        {
            pelAdditionalData.emplace_back(
                "CHIP_TYPE", std::to_string(fapi2::TARGET_TYPE_OCMB_CHIP));
        }

        // Check the error type
        if (sbeError.errType() == openpower::phal::exception::SBE_CMD_TIMEOUT)
        {
            event = sbeTypeAttributes.at(sbeType).chipOpTimeout;
            dumpIsRequired = true;
            // For timeout, we do not expect any FFDC packets
        }
        else if (sbeError.errType() ==
                 openpower::phal::exception::SBE_FFDC_NO_DATA)
        {
            // We will create a PEL without FFDC with the common information we
            // added
            lg2::error("No FFDC data after a chip-op failure {CHIP} {POSITION}",
                       "CHIP", chipName, "POSITION", chipPos);
            event = sbeTypeAttributes.at(sbeType).noFfdc;
        }
        else
        {
            if (sbeError.errType() ==
                openpower::phal::exception::SBE_INTERNAL_FFDC_DATA)
            {
                lg2::info(
                    "FFDC Not related to chip-op present {CHIP} {POSITION}",
                    "CHIP", chipName, "POSITION", chipPos);
                event = sbeTypeAttributes.at(sbeType).sbeInternalFFDCData;
                isDumpFailure = false;
            }
            else
            {
                lg2::error("Process FFDC {CHIP} {POSITION}", "CHIP", chipName,
                           "POSITION", chipPos);
            }
            // Processor FFDC Packets
            openpower::dump::pel::processFFDCPackets(sbeError, event,
                                                     pelAdditionalData);
        }

        // If dump is required, request it
        if (dumpIsRequired)
        {
            auto logId = openpower::dump::pel::createSbeErrorPEL(
                event, sbeError, pelAdditionalData);
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
        lg2::error("SBE Dump request failed, chip type({CHIPTYPE}) "
                   "position({CHIPPOS}), Error: {ERROR}",
                   "CHIPTYPE", chipName, "CHIPPOS", chipPos, "ERROR", e);
    }

    return isDumpFailure;
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
        "Collecting dump from ({CHIPTYPE}) ({POSITION}): path({PATH}) id({ID}) "
        "type({TYPE})  clockState({CLOCKSTATE}) failingUnit({FAILINGUNIT})",
        "CHIPTYPE", chipName, "POSITION", chipPos, "PATH", path.string(), "ID",
        id, "TYPE", type, "CLOCKSTATE", clockState, "FAILINGUNIT", failingUnit);

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

        // If the FFDC is from actual chip-op failure this function will
        // return true, if the chip-op is not failed but FFDC is present
        // then create PELs with FFDC but write the dump contents to the
        // file.
        if (logErrorAndCreatePEL(sbeError, chipPos, sbeType,
                                 SBEFIFO_CMD_CLASS_DUMP, SBEFIFO_CMD_GET_DUMP))
        {
            lg2::error("Error in collecting dump dump type({TYPE}), "
                       "clockstate({CLOCKSTATE}), chip type({CHIPTYPE}) "
                       "position({POSITION}), "
                       "collectFastArray({COLLECTFASTARRAY}) error({ERROR})",
                       "TYPE", type, "CLOCKSTATE", clockState, "CHIPTYPE",
                       chipName, "POSITION", chipPos, "COLLECTFASTARRAY",
                       collectFastArray, "ERROR", sbeError);
            return;
        }
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
    filenameBuilder << std::hex << std::setw(8) << std::setfill('0') << id
                    << ".SbeDataClocks"
                    << (clockState == SBE_CLOCK_ON ? "On" : "Off") << ".node"
                    << std::dec << static_cast<int>(nodeNum) << "." << chipName
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

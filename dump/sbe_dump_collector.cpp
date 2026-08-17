#include "sbe_dump_collector.hpp"

#include "chipop_iface.hpp"
#include "error_iface.hpp"
#include "sbe_consts.hpp"
#include "sbe_type.hpp"
#include "targeting_iface.hpp"

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
#include <future>
#include <iomanip>
#include <map>
#include <span>
#include <sstream>
#include <stdexcept>

namespace openpower::dump::sbe_chipop
{

using namespace phosphor::logging;
using namespace openpower::dump::SBE;

namespace phal_tgt = openpower::dump::phal::targeting;
namespace phal_chipop = openpower::dump::phal::chipop;
namespace phal_err = openpower::dump::phal::error;

void SbeDumpCollector::collectDump(uint8_t type, uint32_t id,
                                   uint32_t failingUnit,
                                   const std::filesystem::path& path)
{
    if ((type == SBE_DUMP_TYPE_SBE) || (type == SBE_DUMP_TYPE_MSBE))
    {
#ifdef LEGACY_PHAL
        // SBE dump collection uses legacy HWPs (libipl/libphal).
        // Not yet implemented for the next backend.
        collectSBEDump(id, failingUnit, path, static_cast<int>(type));
#else
        throw std::runtime_error(
            "P11 SBE dump collection requires the phal-next trigger flow");
#endif
        return;
    }
    collectHWHBDump(type, id, failingUnit, path);
}

void SbeDumpCollector::collectHWHBDump(uint8_t type, uint32_t id,
                                       uint64_t failingUnit,
                                       const std::filesystem::path& path)
{
    lg2::error("Starting dump collection: type:{TYPE} id:{ID} "
               "failingUnit:{FAILINGUNIT}, path:{PATH}",
               "TYPE", type, "ID", id, "FAILINGUNIT", failingUnit, "PATH",
               path.string());

    initializePhalAbstraction();

    TargetMap targets;

    auto primaryTargets = phal_tgt::getPrimaryTargets();
    if (primaryTargets.empty())
    {
        throw std::runtime_error("No functional dump targets found");
    }

    for (auto target : primaryTargets)
    {
        bool includeTarget = true;
        // if the dump type is hostboot then call stop instructions
        if (type == SBE_DUMP_TYPE_HOSTBOOT)
        {
            includeTarget = executeThreadStop(target, path);
        }
        if (includeTarget)
        {
            targets[target] = {};

            // Hardware dumps include the backend's associated chips.
            if (type == openpower::dump::SBE::SBE_DUMP_TYPE_HARDWARE)
            {
                targets[target] = phal_tgt::getAssociatedTargets(target);
            }
        }
    }

    if (targets.empty())
    {
        throw std::runtime_error("No usable primary dump targets found");
    }

    std::vector<uint8_t> clockStates = {SBE_CLOCK_ON, SBE_CLOCK_OFF};
    for (auto cstate : clockStates)
    {
        // Performance dumps are defined only for clocks-on. Hardware and
        // Hostboot dumps retain both passes, including the clocks-off fast
        // array collection on the failing unit.
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
                future.get();
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
    bool hasDumpData = false;
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        if (entry.path().filename() != "errorInfo")
        {
            hasDumpData = true;
            break;
        }
    }
    if (!hasDumpData)
    {
        lg2::error("Failed to collect the dump");
        throw std::runtime_error("Failed to collect the dump");
    }
    lg2::info("Dump collection completed");
}

#ifdef LEGACY_PHAL
void SbeDumpCollector::collectSBEDump(uint32_t id, uint32_t failingUnit,
                                      const std::filesystem::path& dumpPath,
                                      const int sbeTypeId)
{
    lg2::info("Collecting SBE dump: path={PATH}, id={ID}, "
              "chip position={FAILINGUNIT}",
              "PATH", dumpPath.string(), "ID", id, "FAILINGUNIT", failingUnit);

    phal_tgt::TargetHandle proc_ody = nullptr;
    phal_tgt::TargetHandle pibFsiTarget = nullptr;
    std::string sbeChipType;

    try
    {
        // Initialize pdbg + EKB for SBE dump collection
        phal_chipop::initSbeCollection();

        proc_ody = phal_chipop::getTargetForSBEDump(failingUnit, sbeTypeId);

        if (sbeTypeId == phal_chipop::SBE_TYPE_PROC)
        {
            pibFsiTarget =
                phal_chipop::probeSbeTarget(proc_ody, "pib", sbeTypeId);
            sbeChipType = "_p10_";
        }
        else
        {
            pibFsiTarget =
                phal_chipop::probeSbeTarget(proc_ody, "fsi", sbeTypeId);
            sbeChipType = "_ody_";
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to collect the SBE dump: {ERROR}", "ERROR",
                   e.what());
        throw;
    }

    std::stringstream ss;
    ss << std::setw(8) << std::setfill('0') << id;

    std::string baseFilename = ss.str() + ".0_" + std::to_string(failingUnit) +
                               "_SbeData" + sbeChipType;

    try
    {
        phal_chipop::checkSbeState(pibFsiTarget, sbeTypeId);

        phal_chipop::sbeExtractRC(proc_ody, dumpPath, sbeTypeId);

        // Collect various register and memory dumps
        phal_chipop::collectLocalRegDump(proc_ody, dumpPath, baseFilename,
                                         sbeTypeId);
        phal_chipop::collectPIBMSRegDump(proc_ody, dumpPath, baseFilename,
                                         sbeTypeId);
        phal_chipop::collectPIBMEMDump(proc_ody, dumpPath, baseFilename,
                                       sbeTypeId);
        phal_chipop::collectPPEState(proc_ody, dumpPath, baseFilename,
                                     sbeTypeId);

        // Finalize — indicate successful collection
        phal_chipop::finalizeSbeCollection(pibFsiTarget, dumpPath, true,
                                           sbeTypeId);

        lg2::info("SBE dump collection completed successfully");
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to collect the SBE dump: {ERROR}", "ERROR",
                   e.what());
        // Attempt to finalize with failure state so SBE can recover
        if (proc_ody != nullptr)
        {
            try
            {
                phal_chipop::finalizeSbeCollection(pibFsiTarget, dumpPath,
                                                   false, sbeTypeId);
            }
            catch (const std::exception& fe)
            {
                lg2::error("finalizeSbeCollection also failed: {ERROR}",
                           "ERROR", fe.what());
            }
        }
        throw;
    }
}
#endif

void SbeDumpCollector::initializePhalAbstraction()
{
    // Delegate to the abstraction layer's init() which calls:
    // - Legacy backend: openpower::phal::pdbg::init()
    // - P11/PST backend: TARGETING::utils::targetingInit()
    phal_tgt::init();
}

std::vector<std::future<void>> SbeDumpCollector::spawnDumpCollectionProcesses(
    uint8_t type, uint32_t id, const std::filesystem::path& path,
    uint64_t failingUnit, uint8_t cstate, const TargetMap& targetMap)
{
    std::vector<std::future<void>> futures;

    for (const auto& [primaryTarget, associatedTargets] : targetMap)
    {
        auto future = std::async(
            std::launch::async,
            [this, primaryTarget, associatedTargets, path, id, type, cstate,
             failingUnit]() {
            try
            {
                this->collectDumpFromSBE(primaryTarget, path, id, type, cstate,
                                         failingUnit);
            }
            catch (const std::exception& e)
            {
                lg2::error(
                    "Failed to collect dump from primary target "
                    "({POSITION}): {ERROR}",
                    "POSITION", phal_tgt::chipPos(primaryTarget), "ERROR",
                    e.what());
            }

            // Associated chips are collected only with clocks running.
            if (cstate == SBE_CLOCK_ON)
            {
                // Associated chips share their primary target's worker to
                // avoid concurrent chip-ops over the same transport.
                for (auto associatedTarget : associatedTargets)
                {
                    try
                    {
                        this->collectDumpFromSBE(associatedTarget, path, id,
                                                 type,
                                                 cstate, failingUnit);
                    }
                    catch (const std::exception& e)
                    {
                        lg2::error(
                            "Failed to collect dump from associated target "
                            "({POSITION}): {ERROR}",
                            "POSITION", phal_tgt::chipPos(associatedTarget),
                            "ERROR", e.what());
                    }
                }
            }
        });

        futures.push_back(std::move(future));
    }

    return futures;
}

// Unified implementation using ChipOpError abstraction layer type.
// Both backends translate their native error types to ChipOpError.
bool SbeDumpCollector::logErrorAndCreatePEL(
    const phal_chipop::ChipOpError& chipOpError,
    phal_tgt::TargetHandle chipTarget, SBETypes sbeType,
    uint32_t cmdClass, uint32_t cmdType,
    const std::filesystem::path& path)
{
    auto chipPos = phal_tgt::chipPos(chipTarget);
    std::string chipName = phal_tgt::getChipName(chipTarget);
    bool isDumpFailure = true;
    try
    {
        const auto& attrs = sbeTypeAttributes.at(sbeType);

        lg2::info("Chip-op error on {CHIP} position {POSITION}: {ERROR}",
                  "CHIP", chipName, "POSITION", chipPos, "ERROR",
                  chipOpError.what());

        // Select the appropriate D-Bus event name based on error type
        std::string event;
        if (chipOpError.type == phal_chipop::ChipOpError::Type::Timeout)
        {
            event = attrs.chipOpTimeout;
            isDumpFailure = true;
            lg2::error("Chip-op timeout on {CHIP} position {POSITION}", "CHIP",
                       chipName, "POSITION", chipPos);
        }
        else if (chipOpError.type == phal_chipop::ChipOpError::Type::NotAllowed)
        {
            // SBE not ready — informational, not a dump failure
            event = attrs.chipOpFailure;
            isDumpFailure = false;
            lg2::info("Chip-op not allowed on {CHIP} position {POSITION} "
                      "- SBE not ready",
                      "CHIP", chipName, "POSITION", chipPos);
        }
        else if (chipOpError.type == phal_chipop::ChipOpError::Type::NoFfdc)
        {
            event = attrs.noFfdc;
            isDumpFailure = true;
            lg2::error("No FFDC data after chip-op failure on {CHIP} "
                       "position {POSITION}",
                       "CHIP", chipName, "POSITION", chipPos);
        }
        else if (chipOpError.type ==
                 phal_chipop::ChipOpError::Type::InternalFfdc)
        {
            event = attrs.sbeInternalFFDCData;
            isDumpFailure = false;
            lg2::info("Internal FFDC (not chip-op failure) on {CHIP} "
                      "position {POSITION}",
                      "CHIP", chipName, "POSITION", chipPos);
        }
        else
        {
            event = attrs.chipOpFailure;
            isDumpFailure = true;
        }

        if (chipTarget != nullptr && !event.empty())
        {
            auto logIds = phal_err::createChipOpErrorPELs(
                chipOpError, chipTarget, event, cmdClass, cmdType, path);
            for (const auto logId : logIds)
            {
                try
                {
                    auto [pelId, src] = phal_err::getPelInfo(logId);
                    if (pelId != 0)
                    {
                        addLogDataToDump(pelId, src, chipName, chipPos, path);
                    }
                }
                catch (const std::exception& e)
                {
                    lg2::error("Failed to add log data to dump: {ERROR}",
                               "ERROR", e.what());
                }
            }
        }
    }
    catch (const std::out_of_range& e)
    {
        lg2::error("Unknown SBE Type({SBETYPE}) ErrorMsg({ERROR})", "SBETYPE",
                   sbeType, "ERROR", e.what());
    }
    catch (const std::exception& e)
    {
        lg2::error("SBE Dump request failed, chip type({CHIPTYPE}) "
                   "position({CHIPPOS}), Error: {ERROR}",
                   "CHIPTYPE", chipName, "CHIPPOS", chipPos, "ERROR",
                   e.what());
    }
    return isDumpFailure;
}

void SbeDumpCollector::collectDumpFromSBE(
    phal::targeting::TargetHandle chip, const std::filesystem::path& path,
    uint32_t id, uint8_t type, uint8_t clockState, uint64_t failingUnit)
{
    auto chipPos = phal_tgt::chipPos(chip);
    SBETypes sbeType = getSBEType(chip);
    auto chipName = phal_tgt::getChipName(chip);
    lg2::info(
        "Collecting dump from ({CHIPTYPE}) ({POSITION}): path({PATH}) id({ID}) "
        "type({TYPE})  clockState({CLOCKSTATE}) failingUnit({FAILINGUNIT})",
        "CHIPTYPE", chipName, "POSITION", chipPos, "PATH", path.string(), "ID",
        id, "TYPE", type, "CLOCKSTATE", clockState, "FAILINGUNIT", failingUnit);

    uint8_t collectFastArray =
        checkFastarrayCollectionNeeded(clockState, type, failingUnit, chipPos);

    try
    {
        // Use abstraction layer to get dump; DumpData owns the buffer
        auto dumpData =
            phal_chipop::getDump(chip, type, clockState, collectFastArray);

        auto node = phal_tgt::nodePos(chip);
        writeDumpFile(path, id, clockState, node, chipName, chipPos,
                      dumpData.bytes());
    }
    catch (const phal_chipop::ChipOpError& chipOpError)
    {
        if (chipOpError.type == phal_chipop::ChipOpError::Type::NotAllowed)
        {
            // SBE is not ready to accept chip-ops — skip, no PEL needed
            lg2::info("Collect dump: Skipping ({ERROR}) dump({TYPE}) "
                      "on proc({PROC}) clock state({CLOCKSTATE})",
                      "ERROR", chipOpError.what(), "TYPE", type, "PROC",
                      chipPos, "CLOCKSTATE", clockState);
            return;
        }

        // Use logErrorAndCreatePEL() which handles PEL creation + errorInfo
        // file
        bool isDumpFailure = logErrorAndCreatePEL(
            chipOpError, chip, sbeType, SBEFIFO_CMD_CLASS_DUMP,
            SBEFIFO_CMD_GET_DUMP, path);

        if (isDumpFailure)
        {
            lg2::error("Error in collecting dump dump type({TYPE}), "
                       "clockstate({CLOCKSTATE}), chip type({CHIPTYPE}) "
                       "position({POSITION}), "
                       "collectFastArray({COLLECTFASTARRAY}) error({ERROR})",
                       "TYPE", type, "CLOCKSTATE", clockState, "CHIPTYPE",
                       chipName, "POSITION", chipPos, "COLLECTFASTARRAY",
                       collectFastArray, "ERROR", chipOpError.what());
            return;
        }

        if (chipOpError.data && chipOpError.data->size() != 0)
        {
            auto node = phal_tgt::nodePos(chip);
            writeDumpFile(path, id, clockState, node, chipName, chipPos,
                          chipOpError.data->bytes());
        }
    }
}

void SbeDumpCollector::writeDumpFile(
    const std::filesystem::path& path, const uint32_t id,
    const uint8_t clockState, const uint32_t nodeNum,
    const std::string& chipName, const uint32_t chipPos,
    std::span<const uint8_t> bytes)
{
    using namespace sdbusplus::xyz::openbmc_project::Common::Error;

    // Construct the filename
    std::ostringstream filenameBuilder;
    filenameBuilder << std::hex << std::setw(8) << std::setfill('0') << id
                    << ".SbeDataClocks"
                    << (clockState == SBE_CLOCK_ON ? "On" : "Off") << ".node"
                    << std::dec << nodeNum << "." << chipName << chipPos;

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
        outfile.write(reinterpret_cast<const char*>(bytes.data()),
                      bytes.size());

        lg2::info("Successfully wrote dump file "
                  "path=({PATH}) size=({SIZE})",
                  "PATH", dumpPath.string(), "SIZE", bytes.size());
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

bool SbeDumpCollector::executeThreadStop(phal_tgt::TargetHandle target,
                                         const std::filesystem::path& path)
{
    try
    {
        phal_chipop::threadStopProc(target);
        return true;
    }
    catch (const phal_chipop::ChipOpError& chipOpError)
    {
        uint64_t chipPos = phal_tgt::chipPos(target);

        if (chipOpError.type == phal_chipop::ChipOpError::Type::NotAllowed)
        {
            lg2::info("SBE is not ready to accept chip-op: Skipping "
                      "stop instruction on proc-({POSITION}) error({ERROR}) ",
                      "POSITION", chipPos, "ERROR", chipOpError.what());
            return false; // Do not include the target for dump collection
        }

        lg2::error("Stop instructions failed on "
                   "proc-({POSITION}) error({ERROR}) ",
                   "POSITION", chipPos, "ERROR", chipOpError.what());

        // Use logErrorAndCreatePEL() for PEL creation + errorInfo file
        logErrorAndCreatePEL(chipOpError, target, SBETypes::PROC,
                             SBEFIFO_CMD_CLASS_INSTRUCTION,
                             SBEFIFO_CMD_CONTROL_INSN, path);

        // For TIMEOUT, skip adding the processor for dump collection
        if (chipOpError.type == phal_chipop::ChipOpError::Type::Timeout)
        {
            return false;
        }
    }
    // Include the target for dump collection for FAILED or any other
    // non-critical errors
    return true;
}

void SbeDumpCollector::addLogDataToDump(uint32_t pelId, std::string src,
                                        std::string chipName, uint64_t chipPos,
                                        const std::filesystem::path& path)
{
    std::filesystem::path info = path / "errorInfo";
    auto fileExists = std::filesystem::exists(info);
    std::ofstream fout;
    fout.open(info, std::ios::app);
    if (!fout)
    {
        lg2::error("Error: Failed to open the file! {FILE}", "FILE", info);
        lg2::error("No error Info is added to dump file");
        return;
    }
    if (!fileExists)
    {
        fout << "ErrorInfo:" << std::endl;
    }
    auto pel = " " + std::format("{:08x}", pelId) + ":";
    fout << pel << std::endl;
    fout << "  src: " << src << std::endl;
    auto resource = chipName + " " + std::to_string(chipPos);
    fout << "  Resource: " << resource << std::endl;
}

} // namespace openpower::dump::sbe_chipop

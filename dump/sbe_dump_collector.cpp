#include "sbe_consts.hpp"
#include "sbe_dump_collector.hpp"
#include "sbe_type.hpp"

// Use PHAL abstraction layer
#include "../phal/targeting_iface.hpp"
#include "../phal/chipop_iface.hpp"
#include "../phal/error_iface.hpp"

// create_pel.hpp provides getLogInfo() (available on both backends)
// and createSbeErrorPEL() / processFFDCPackets() (legacy only, guarded inside)
#include "create_pel.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>
#include <phosphor-logging/log.hpp>
#include <sbe_consts.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdint>
#include <filesystem>
#include <format>
#include <span>
#include <fstream>
#include <future>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

namespace openpower::dump::sbe_chipop
{

using namespace phosphor::logging;
using namespace openpower::dump::SBE;

// Namespace aliases for PHAL abstraction layer
namespace phal_tgt = openpower::dump::phal::targeting;
namespace phal_cop = openpower::dump::phal::chipop;
namespace phal_err = openpower::dump::phal::error;

void SbeDumpCollector::collectDump(uint8_t type, uint32_t id,
                                   uint32_t failingUnit,
                                   const std::filesystem::path& path)
{
    if ((type == SBE_DUMP_TYPE_SBE) || (type == SBE_DUMP_TYPE_MSBE))
    {
#ifdef USE_PHAL_OLD
        // SBE dump collection uses legacy HWPs (libipl/libphal).
        // Not yet implemented for the next backend.
        collectSBEDump(id, failingUnit, path, static_cast<int>(type));
#else
        lg2::error("SBE dump collection not supported on this backend "
                   "(type={TYPE})", "TYPE", type);
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

    initializePdbg();

    TargetMap targets;

    // getAllProcTargets() already returns only functional+probed targets
    auto procTargets = phal_tgt::getAllProcTargets();

    for (auto target : procTargets)
    {
        bool includeTarget = true;
        // if the dump type is hostboot then call stop instructions
        if (type == SBE_DUMP_TYPE_HOSTBOOT)
        {
            includeTarget = executeThreadStop(target, path);
        }
        if (includeTarget)
        {
            targets[target] = std::vector<phal_tgt::TargetHandle>();

            // Hardware dump needs OCMB data if present
            if (type == openpower::dump::SBE::SBE_DUMP_TYPE_HARDWARE)
            {
                // getAllOCMBTargets() already returns only functional Odyssey OCMBs
                auto ocmbTargets = phal_tgt::getAllOCMBTargets(target);
                targets[target] = ocmbTargets;
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

#ifdef USE_PHAL_OLD
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
        phal_cop::initSbeCollection();

        proc_ody = phal_cop::getTargetForSBEDump(failingUnit, sbeTypeId);

        if (sbeTypeId == phal_cop::SBE_TYPE_PROC)
        {
            pibFsiTarget = phal_cop::probeSbeTarget(proc_ody, "pib", sbeTypeId);
            sbeChipType = "_p10_";
        }
        else
        {
            pibFsiTarget = phal_cop::probeSbeTarget(proc_ody, "fsi", sbeTypeId);
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
        phal_cop::checkSbeState(pibFsiTarget, sbeTypeId);

        phal_cop::sbeExtractRC(proc_ody, dumpPath, sbeTypeId);

        // Collect various register and memory dumps
        phal_cop::collectLocalRegDump(proc_ody, dumpPath, baseFilename,
                                      sbeTypeId);
        phal_cop::collectPIBMSRegDump(proc_ody, dumpPath, baseFilename,
                                      sbeTypeId);
        phal_cop::collectPIBMEMDump(proc_ody, dumpPath, baseFilename,
                                    sbeTypeId);
        phal_cop::collectPPEState(proc_ody, dumpPath, baseFilename, sbeTypeId);

        // Finalize — indicate successful collection
        phal_cop::finalizeSbeCollection(pibFsiTarget, dumpPath, true,
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
                phal_cop::finalizeSbeCollection(pibFsiTarget, dumpPath, false,
                                                sbeTypeId);
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
#else
void SbeDumpCollector::collectSBEDump(
    [[maybe_unused]] uint32_t id,
    [[maybe_unused]] uint32_t failingUnit,
    [[maybe_unused]] const std::filesystem::path& dumpPath,
    [[maybe_unused]] const int sbeTypeId)
{
    // SBE dump collection is not implemented for the next backend.
    // collectDump() guards this call with #ifdef USE_PHAL_OLD.
    lg2::error("collectSBEDump: not supported on this backend");
    throw std::runtime_error("SBE dump collection not supported on this backend");
}
#endif

void SbeDumpCollector::initializePdbg()
{
    // Delegate to the abstraction layer's init() which calls
    // openpower::phal::pdbg::init() (legacy) or
    // TARGETING::utils::targetingInit() (next)
    phal_tgt::init();
}

std::vector<std::future<void>> SbeDumpCollector::spawnDumpCollectionProcesses(
    uint8_t type, uint32_t id, const std::filesystem::path& path,
    uint64_t failingUnit, uint8_t cstate, const TargetMap& targetMap)
{
    std::vector<std::future<void>> futures;

    for (const auto& [procTarget, ocmbTargets] : targetMap)
    {
        auto future = std::async(std::launch::async, [this, procTarget,
                                                      ocmbTargets, path, id,
                                                      type, cstate,
                                                      failingUnit]() {
            try
            {
                this->collectDumpFromSBE(procTarget, path, id, type, cstate,
                                         failingUnit);
            }
            catch (const std::exception& e)
            {
                lg2::error(
                    "Failed to collect dump from SBE on Proc-({PROCINDEX}) {ERROR}",
                    "PROCINDEX", phal_tgt::chipPos(procTarget), "ERROR", e);
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
                            "OCMBINDEX", phal_tgt::chipPos(ocmbTarget), "ERROR",
                            e);
                    }
                }
            }
        });

        futures.push_back(std::move(future));
    }

    return futures;
}

// Unified implementation using ChipOpError abstraction layer type.
// Both backends translate their native error types to ChipOpError in
// chipop_old.cpp / chipop_next.cpp before reaching this function.
bool SbeDumpCollector::logErrorAndCreatePEL(
    const phal_cop::ChipOpError& chipOpError,
    uint64_t chipPos,
    SBETypes sbeType,
    [[maybe_unused]] uint32_t cmdClass,
    [[maybe_unused]] uint32_t cmdType,
    const std::filesystem::path& path)
{
    std::string chipName;
    bool isDumpFailure = true;
    try
    {
        const auto& attrs = sbeTypeAttributes.at(sbeType);
        chipName = attrs.chipName;

        lg2::info("Chip-op error on {CHIP} position {POSITION}: {ERROR}",
                  "CHIP", chipName, "POSITION", chipPos, "ERROR",
                  chipOpError.what());

        // Select the appropriate D-Bus event name based on error type
        std::string event;
        if (chipOpError.type == phal_cop::ChipOpError::Type::Timeout)
        {
            event = attrs.chipOpTimeout;
            isDumpFailure = true;
            lg2::error("Chip-op timeout on {CHIP} position {POSITION}",
                       "CHIP", chipName, "POSITION", chipPos);
        }
        else if (chipOpError.type == phal_cop::ChipOpError::Type::NotAllowed)
        {
            // SBE not ready — informational, not a dump failure
            event = attrs.chipOpFailure;
            isDumpFailure = false;
            lg2::info("Chip-op not allowed on {CHIP} position {POSITION} "
                      "- SBE not ready",
                      "CHIP", chipName, "POSITION", chipPos);
        }
        else if (chipOpError.type == phal_cop::ChipOpError::Type::NoFfdc)
        {
            event = attrs.noFfdc;
            isDumpFailure = true;
            lg2::error("No FFDC data after chip-op failure on {CHIP} "
                       "position {POSITION}",
                       "CHIP", chipName, "POSITION", chipPos);
        }
        else if (chipOpError.type == phal_cop::ChipOpError::Type::InternalFfdc)
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

        // Find the target handle for this chip position so we can pass it
        // to createChipOpErrorPEL.  We look up by position in the proc list
        // first, then OCMB list.
        phal_tgt::TargetHandle chipTarget = nullptr;
        {
            auto procs = phal_tgt::getAllProcTargets();
            for (auto t : procs)
            {
                if (phal_tgt::chipPos(t) == static_cast<uint32_t>(chipPos))
                {
                    chipTarget = t;
                    break;
                }
            }
            if (chipTarget == nullptr)
            {
                // Try OCMB targets under each proc
                for (auto proc : procs)
                {
                    for (auto t : phal_tgt::getAllOCMBTargets(proc))
                    {
                        if (phal_tgt::chipPos(t) ==
                            static_cast<uint32_t>(chipPos))
                        {
                            chipTarget = t;
                            break;
                        }
                    }
                    if (chipTarget != nullptr)
                        break;
                }
            }
        }

        if (chipTarget != nullptr && !event.empty())
        {
            // Create PEL via abstraction layer
            uint32_t logId = phal_err::createChipOpErrorPEL(
                chipOpError, chipTarget, event, path);

#ifdef USE_PHAL_OLD
            // getLogInfo() is defined in create_pel.cpp (legacy only).
            // For the next backend, createChipOpErrorPEL() returns 0 anyway.
            if (logId != 0)
            {
                // Retrieve PEL ID + SRC and write to dump errorInfo file
                // (best-effort — don't fail dump collection if this fails)
                try
                {
                    auto [pelId, src] =
                        openpower::dump::pel::getLogInfo(logId);
                    addLogDataToDump(pelId, src, chipName, chipPos, path);
                }
                catch (const std::exception& e)
                {
                    lg2::error("Failed to add log data to dump: {ERROR}",
                               "ERROR", e.what());
                }
            }
#else
            (void)logId; // next backend: PEL creation not yet implemented
#endif
        }
    }
    catch (const std::out_of_range& e)
    {
        lg2::error("Unknown SBE Type({SBETYPE}) ErrorMsg({ERROR})",
                   "SBETYPE", sbeType, "ERROR", e);
    }
    catch (const std::exception& e)
    {
        lg2::error("SBE Dump request failed, chip type({CHIPTYPE}) "
                   "position({CHIPPOS}), Error: {ERROR}",
                   "CHIPTYPE", chipName, "CHIPPOS", chipPos, "ERROR", e);
    }
    return isDumpFailure;
}

void SbeDumpCollector::collectDumpFromSBE(
    phal::targeting::TargetHandle chip, const std::filesystem::path& path, uint32_t id,
    uint8_t type, uint8_t clockState, uint64_t failingUnit)
{
    auto chipPos = phal_tgt::chipPos(chip);
    SBETypes sbeType = getSBEType(chip);
    auto chipName = sbeTypeAttributes.at(sbeType).chipName;
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
        auto dumpData = phal_cop::getDump(chip, type, clockState, collectFastArray);

        // Pass byte span directly — no extra copy needed
        writeDumpFile(path, id, clockState, 0, chipName, chipPos,
                      dumpData.bytes());
    }
    catch (const phal_cop::ChipOpError& chipOpError)
    {
        if (chipOpError.type == phal_cop::ChipOpError::Type::NotAllowed)
        {
            // SBE is not ready to accept chip-ops — skip, no PEL needed
            lg2::info("Collect dump: Skipping ({ERROR}) dump({TYPE}) "
                      "on proc({PROC}) clock state({CLOCKSTATE})",
                      "ERROR", chipOpError.what(), "TYPE", type, "PROC", chipPos,
                      "CLOCKSTATE", clockState);
            return;
        }

        // Use logErrorAndCreatePEL() which handles PEL creation + errorInfo file
        bool isDumpFailure = logErrorAndCreatePEL(chipOpError, chipPos, sbeType,
                                                   SBEFIFO_CMD_CLASS_DUMP,
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
    }
}

void SbeDumpCollector::writeDumpFile(
    const std::filesystem::path& path, const uint32_t id,
    const uint8_t clockState, const uint8_t nodeNum,
    const std::string& chipName, const uint8_t chipPos,
    std::span<const uint8_t> bytes)
{
    using namespace sdbusplus::xyz::openbmc_project::Common::Error;

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
        outfile.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());

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
        phal_cop::threadStopProc(target);
        return true;
    }
    catch (const std::runtime_error& e)
    {
        // phal_next Phase 1: threadStopProc not yet implemented
        uint64_t chipPos = phal_tgt::chipPos(target);
        lg2::error("Thread stop not implemented: proc-({POSITION}) {ERROR}",
                   "POSITION", chipPos, "ERROR", e.what());
        // Fail hostboot dumps gracefully for phal_next Phase 1
        return false;
    }
    catch (const phal_cop::ChipOpError& chipOpError)
    {
        uint64_t chipPos = phal_tgt::chipPos(target);

        if (chipOpError.type == phal_cop::ChipOpError::Type::NotAllowed)
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
        logErrorAndCreatePEL(chipOpError, chipPos, SBETypes::PROC,
                             SBEFIFO_CMD_CLASS_INSTRUCTION,
                             SBEFIFO_CMD_CONTROL_INSN, path);

        // For TIMEOUT, skip adding the processor for dump collection
        if (chipOpError.type == phal_cop::ChipOpError::Type::Timeout)
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

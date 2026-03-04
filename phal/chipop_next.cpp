#ifdef USE_PHAL_NEXT

#include "chipop_iface.hpp"
#include <phosphor-logging/lg2.hpp>
#include <stdexcept>
#include <string>
#include <vector>

// Required hostfw/phal headers for next-gen chip operations
// Headers are installed flat to ${includedir} (no subdir prefix)
// sbe_cmd_impl.H is a template header that transitively includes fapi2.H
#include <sbe_cmd_impl.H>
#include <return_code.H>

namespace openpower::dump::phal::chipop {

// ============================================================================
// DumpData Implementation (same as old backend - reusable)
// ============================================================================

DumpData DumpData::fromMalloc(uint8_t* p, uint32_t len)
{
    DumpData d;
    MallocBuf buf;
    buf.p = p;
    buf.len = len;
    d.storage_ = std::move(buf);
    return d;
}

DumpData DumpData::fromVector(std::vector<uint8_t>&& v)
{
    DumpData d;
    d.storage_ = std::move(v);
    return d;
}

std::span<const uint8_t> DumpData::bytes() const
{
    if (std::holds_alternative<MallocBuf>(storage_))
    {
        const auto& buf = std::get<MallocBuf>(storage_);
        return std::span<const uint8_t>(buf.p, buf.len);
    }
    else if (std::holds_alternative<std::vector<uint8_t>>(storage_))
    {
        const auto& vec = std::get<std::vector<uint8_t>>(storage_);
        return std::span<const uint8_t>(vec.data(), vec.size());
    }
    return {};
}

uint32_t DumpData::size() const
{
    if (std::holds_alternative<MallocBuf>(storage_))
    {
        return std::get<MallocBuf>(storage_).len;
    }
    else if (std::holds_alternative<std::vector<uint8_t>>(storage_))
    {
        return std::get<std::vector<uint8_t>>(storage_).size();
    }
    return 0;
}

// ============================================================================
// Chip Operations
// ============================================================================

DumpData getDump(targeting::TargetHandle chip,
                 uint8_t dumpType,
                 uint8_t clockState,
                 uint8_t collectFastArray)
{
    // sbei::getDump() retrieves dump data from SBE via FIFO interface.
    // It requires a fapi2::Target wrapper and returns data via output parameter.
    
    std::vector<sbei::ResponseInfo::FFDC> ffdc;
    std::vector<uint8_t> dumpData;
    
    // Wrap the raw target pointer in fapi2::Target for sbei API
    fapi2::Target<fapi2::TARGET_TYPE_PROC_CHIP> fapiChip(chip);
    
    auto rc = sbei::getDump(fapiChip, dumpType, clockState,
                            collectFastArray, dumpData, ffdc);
    
    if (rc != fapi2::FAPI2_RC_SUCCESS)
    {
        // Translate fapi2::ReturnCode to ChipOpError
        // The FFDC vector contains detailed error information
        std::string msg = "getDump failed with RC=" + std::to_string(static_cast<uint32_t>(rc));
        if (!ffdc.empty())
        {
            msg += ", FFDC count=" + std::to_string(ffdc.size());
        }
        
        // Map return codes to error types
        // TODO: Refine error type mapping based on actual RC values
        ChipOpError::Type errType = ChipOpError::Type::Failed;
        
        // Common fapi2 return codes (from return_code.H):
        // FAPI2_RC_SUCCESS = 0
        // Other RCs indicate various failure modes
        if (rc != fapi2::FAPI2_RC_SUCCESS)
        {
            // For now, treat all non-success as Failed
            // Future: parse RC to distinguish Timeout, NotAllowed, etc.
            errType = ChipOpError::Type::Failed;
        }
        
        throw ChipOpError(errType, msg);
    }
    
    // Success: return the collected dump data
    lg2::info("PHAL Next: getDump collected {SIZE} bytes", "SIZE", dumpData.size());
    return DumpData::fromVector(std::move(dumpData));
}

void threadStopProc(targeting::TargetHandle proc)
{
    // phal_next hostboot dump sequence: MPIPL enter → continue → stop clocks
    // This replaces the legacy openpower::phal::sbe::threadStopProc() HWP.
    std::vector<sbei::ResponseInfo::FFDC> ffdc;
    fapi2::Target<fapi2::TARGET_TYPE_PROC_CHIP> fapiProc(proc);

    // Step 1: Enter MPIPL mode
    auto rc = sbei::enterMpiplChipOp(fapiProc, ffdc);
    if (rc != fapi2::FAPI2_RC_SUCCESS)
    {
        std::string msg = "threadStopProc: enterMpipl failed";
        if (!ffdc.empty())
        {
            msg += ": FFDC count=" + std::to_string(ffdc.size());
        }
        throw ChipOpError(ChipOpError::Type::Failed, msg);
    }

    // Step 2: Continue MPIPL (transition to dump-ready state)
    ffdc.clear();
    rc = sbei::continueMpiplChipOp(fapiProc, ffdc);
    if (rc != fapi2::FAPI2_RC_SUCCESS)
    {
        std::string msg = "threadStopProc: continueMpipl failed";
        if (!ffdc.empty())
        {
            msg += ": FFDC count=" + std::to_string(ffdc.size());
        }
        throw ChipOpError(ChipOpError::Type::Failed, msg);
    }

    // Step 3: Stop clocks — logTargetType=0xFF (all), instanceId=0xFF (all)
    ffdc.clear();
    rc = sbei::stopClocksChipOp(fapiProc, /*logTargetType=*/0xFF,
                                 /*instanceId=*/0xFF, ffdc);
    if (rc != fapi2::FAPI2_RC_SUCCESS)
    {
        std::string msg = "threadStopProc: stopClocks failed";
        if (!ffdc.empty())
        {
            msg += ": FFDC count=" + std::to_string(ffdc.size());
        }
        throw ChipOpError(ChipOpError::Type::Failed, msg);
    }
}

// ============================================================================
// SBE Dump Collection HWPs — stubs for next backend
// ============================================================================

void initSbeCollection()
{
    // phal_next: targeting is already initialized by init(); no separate
    // EKB init needed — sbei:: functions use fapi2 targets directly.
    lg2::info("PHAL Next: initSbeCollection — no-op (sbei uses fapi2 targets)");
}

targeting::TargetHandle getTargetForSBEDump(uint32_t failingUnit,
                                             int sbeTypeId)
{
    // phal_next: targets are enumerated via targeting_next.cpp::getAllProcTargets()
    // / getAllOCMBTargets(); collectSBEDump() is guarded by #ifdef USE_PHAL_OLD
    // so this function is never called on the next backend.
    (void)failingUnit;
    (void)sbeTypeId;
    lg2::info("PHAL Next: getTargetForSBEDump (not called on next backend)");
    throw std::runtime_error("getTargetForSBEDump not applicable to phal_next");
}

targeting::TargetHandle probeSbeTarget(targeting::TargetHandle proc,
                                        const std::string& subTarget,
                                        int sbeTypeId)
{
    // phal_next: no pdbg probe concept; sbei:: functions take fapi2 targets
    // directly. collectSBEDump() is guarded by #ifdef USE_PHAL_OLD.
    (void)proc;
    (void)subTarget;
    (void)sbeTypeId;
    lg2::info("PHAL Next: probeSbeTarget (not called on next backend)");
    throw std::runtime_error("probeSbeTarget not applicable to phal_next");
}

void checkSbeState(targeting::TargetHandle target, int sbeTypeId)
{
    // Use getSbeCapabilitiesChipOp() to probe SBE readiness.
    // A failed RC means the SBE is not ready to accept chip-ops.
    (void)sbeTypeId;
    
    std::vector<sbei::ResponseInfo::FFDC> ffdc;
    fapi2::Target<fapi2::TARGET_TYPE_PROC_CHIP> fapiTarget(target);
    auto rc = sbei::getSbeCapabilitiesChipOp(fapiTarget, ffdc);
    if (rc != fapi2::FAPI2_RC_SUCCESS)
    {
        std::string msg = "SBE not ready (getSbeCapabilities failed)";
        if (!ffdc.empty())
        {
            msg += ": FFDC count=" + std::to_string(ffdc.size());
        }
        throw ChipOpError(ChipOpError::Type::NotAllowed, msg);
    }
}

void sbeExtractRC(targeting::TargetHandle proc,
                  const std::filesystem::path& dumpPath,
                  int sbeTypeId)
{
    // getTiInfoChipOp() retrieves TI (Terminate Immediately) data —
    // the phal_next equivalent of extracting the SBE error RC.
    (void)dumpPath;
    (void)sbeTypeId;
    
    std::vector<sbei::ResponseInfo::FFDC> ffdc;
    fapi2::Target<fapi2::TARGET_TYPE_PROC_CHIP> fapiProc(proc);
    auto rc = sbei::getTiInfoChipOp(fapiProc, ffdc);
    if (rc != fapi2::FAPI2_RC_SUCCESS)
    {
        std::string msg = "sbeExtractRC (getTiInfo) failed";
        if (!ffdc.empty())
        {
            msg += ": FFDC count=" + std::to_string(ffdc.size());
        }
        throw ChipOpError(ChipOpError::Type::Failed, msg);
    }
}

void collectLocalRegDump(targeting::TargetHandle proc,
                          const std::filesystem::path& dumpPath,
                          const std::string& baseFilename,
                          int sbeTypeId)
{
    // phal_next: local register dump is collected via getDump() chip-op
    // (collectHWHBDump path), not via separate HWPs. collectSBEDump() is
    // guarded by #ifdef USE_PHAL_OLD so this is never called.
    (void)proc;
    (void)dumpPath;
    (void)baseFilename;
    (void)sbeTypeId;
    lg2::info("PHAL Next: collectLocalRegDump (not called on next backend)");
    throw std::runtime_error("collectLocalRegDump not applicable to phal_next");
}

void collectPIBMSRegDump(targeting::TargetHandle proc,
                          const std::filesystem::path& dumpPath,
                          const std::string& baseFilename,
                          int sbeTypeId)
{
    (void)proc;
    (void)dumpPath;
    (void)baseFilename;
    (void)sbeTypeId;
    lg2::info("PHAL Next: collectPIBMSRegDump (not called on next backend)");
    throw std::runtime_error("collectPIBMSRegDump not applicable to phal_next");
}

void collectPIBMEMDump(targeting::TargetHandle proc,
                        const std::filesystem::path& dumpPath,
                        const std::string& baseFilename,
                        int sbeTypeId)
{
    (void)proc;
    (void)dumpPath;
    (void)baseFilename;
    (void)sbeTypeId;
    lg2::info("PHAL Next: collectPIBMEMDump (not called on next backend)");
    throw std::runtime_error("collectPIBMEMDump not applicable to phal_next");
}

void collectPPEState(targeting::TargetHandle proc,
                     const std::filesystem::path& dumpPath,
                     const std::string& baseFilename,
                     int sbeTypeId)
{
    (void)proc;
    (void)dumpPath;
    (void)baseFilename;
    (void)sbeTypeId;
    lg2::info("PHAL Next: collectPPEState (not called on next backend)");
    throw std::runtime_error("collectPPEState not applicable to phal_next");
}

void finalizeSbeCollection(targeting::TargetHandle target,
                            const std::filesystem::path& dumpPath,
                            bool success,
                            int sbeTypeId)
{
    (void)dumpPath;
    (void)sbeTypeId;
    
    if (!success)
    {
        // On failure path, skip quiesce — SBE may already be in bad state
        lg2::info("PHAL Next: finalizeSbeCollection — skipping quiesce on failure");
        return;
    }
    
    // quiesceSbeChipOp() signals the SBE that collection is complete
    std::vector<sbei::ResponseInfo::FFDC> ffdc;
    fapi2::Target<fapi2::TARGET_TYPE_PROC_CHIP> fapiTarget(target);
    auto rc = sbei::quiesceSbeChipOp(fapiTarget, ffdc);
    if (rc != fapi2::FAPI2_RC_SUCCESS)
    {
        std::string msg = "finalizeSbeCollection (quiesceSbe) failed";
        if (!ffdc.empty())
        {
            msg += ": FFDC count=" + std::to_string(ffdc.size());
        }
        throw ChipOpError(ChipOpError::Type::Failed, msg);
    }
}

} // namespace openpower::dump::phal::chipop

#endif // USE_PHAL_NEXT

// Made with Bob

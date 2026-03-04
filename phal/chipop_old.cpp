#ifdef USE_PHAL_OLD

#include "chipop_iface.hpp"
#include <libphal.H>
#include <phal_exception.H>
#include <phosphor-logging/lg2.hpp>

extern "C" {
#include <libpdbg.h>
}

namespace openpower::dump::phal::chipop {

// ============================================================================
// DumpData Implementation
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
    try
    {
        uint8_t* dumpPtr = nullptr;
        uint32_t dumpLen = 0;
        
        openpower::phal::sbe::getDump(
            chip,
            dumpType,
            clockState,
            collectFastArray,
            &dumpPtr,
            &dumpLen
        );
        
        return DumpData::fromMalloc(dumpPtr, dumpLen);
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        // Translate sbeError_t to ChipOpError
        ChipOpError::Type errType = ChipOpError::Type::Unknown;
        
        if (e.errType() == openpower::phal::exception::SBE_CMD_NOT_ALLOWED)
        {
            errType = ChipOpError::Type::NotAllowed;
        }
        else if (e.errType() == openpower::phal::exception::SBE_CMD_TIMEOUT)
        {
            errType = ChipOpError::Type::Timeout;
        }
        else if (e.errType() == openpower::phal::exception::SBE_FFDC_NO_DATA)
        {
            errType = ChipOpError::Type::NoFfdc;
        }
        else if (e.errType() == openpower::phal::exception::SBE_INTERNAL_FFDC)
        {
            errType = ChipOpError::Type::InternalFfdc;
        }
        else
        {
            errType = ChipOpError::Type::Failed;
        }
        
        throw ChipOpError(errType, e.what());
    }
}

void threadStopProc(targeting::TargetHandle proc)
{
    try
    {
        openpower::phal::sbe::threadStopProc(proc);
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        // Translate sbeError_t to ChipOpError
        ChipOpError::Type errType = ChipOpError::Type::Unknown;
        
        if (e.errType() == openpower::phal::exception::SBE_CMD_NOT_ALLOWED)
        {
            errType = ChipOpError::Type::NotAllowed;
        }
        else if (e.errType() == openpower::phal::exception::SBE_CMD_TIMEOUT)
        {
            errType = ChipOpError::Type::Timeout;
        }
        else
        {
            errType = ChipOpError::Type::Failed;
        }
        
        throw ChipOpError(errType, e.what());
    }
}

// ============================================================================
// SBE Dump Collection HWPs
// ============================================================================

void initSbeCollection()
{
    // Initialize pdbg for SBE dump collection.
    // Uses the same init path as targeting_old.cpp::init().
    // The EKB library is loaded as part of pdbg init via libphal.
    openpower::phal::pdbg::init();
}

targeting::TargetHandle getTargetForSBEDump(uint32_t failingUnit,
                                             int sbeTypeId)
{
    // For PROC SBE: iterate "proc" class targets
    // For OCMB SBE: iterate "ocmb" class targets
    const char* targetClass = (sbeTypeId == SBE_TYPE_OCMB) ? "ocmb" : "proc";

    struct pdbg_target* target = nullptr;
    pdbg_for_each_class_target(targetClass, target)
    {
        if (pdbg_target_index(target) == failingUnit)
        {
            return target;
        }
    }

    throw std::runtime_error(
        std::string("Target not found for failingUnit=") +
        std::to_string(failingUnit) +
        " sbeTypeId=" + std::to_string(sbeTypeId));
}

targeting::TargetHandle probeSbeTarget(targeting::TargetHandle proc,
                                        const std::string& subTarget,
                                        [[maybe_unused]] int sbeTypeId)
{
    struct pdbg_target* child = nullptr;
    pdbg_for_each_target(subTarget.c_str(), proc, child)
    {
        if (pdbg_target_probe(child) == PDBG_TARGET_ENABLED)
        {
            return child;
        }
    }

    throw std::runtime_error(
        std::string("Failed to probe sub-target '") + subTarget +
        "' under proc target");
}

void checkSbeState(targeting::TargetHandle pibFsiTarget,
                   [[maybe_unused]] int sbeTypeId)
{
    try
    {
        openpower::phal::sbe::checkSbeState(pibFsiTarget);
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("SBE state check failed: ") + e.what());
    }
}

void sbeExtractRC(targeting::TargetHandle proc,
                  const std::filesystem::path& dumpPath,
                  [[maybe_unused]] int sbeTypeId)
{
    try
    {
        openpower::phal::sbe::extractRC(proc, dumpPath.string().c_str());
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("SBE extractRC failed: ") + e.what());
    }
}

void collectLocalRegDump(targeting::TargetHandle proc,
                          const std::filesystem::path& dumpPath,
                          const std::string& baseFilename,
                          [[maybe_unused]] int sbeTypeId)
{
    try
    {
        openpower::phal::sbe::collectLocalRegDump(
            proc, dumpPath.string().c_str(), baseFilename.c_str());
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("collectLocalRegDump failed: ") + e.what());
    }
}

void collectPIBMSRegDump(targeting::TargetHandle proc,
                          const std::filesystem::path& dumpPath,
                          const std::string& baseFilename,
                          [[maybe_unused]] int sbeTypeId)
{
    try
    {
        openpower::phal::sbe::collectPIBMSRegDump(
            proc, dumpPath.string().c_str(), baseFilename.c_str());
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("collectPIBMSRegDump failed: ") + e.what());
    }
}

void collectPIBMEMDump(targeting::TargetHandle proc,
                        const std::filesystem::path& dumpPath,
                        const std::string& baseFilename,
                        [[maybe_unused]] int sbeTypeId)
{
    try
    {
        openpower::phal::sbe::collectPIBMEMDump(
            proc, dumpPath.string().c_str(), baseFilename.c_str());
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("collectPIBMEMDump failed: ") + e.what());
    }
}

void collectPPEState(targeting::TargetHandle proc,
                     const std::filesystem::path& dumpPath,
                     const std::string& baseFilename,
                     [[maybe_unused]] int sbeTypeId)
{
    try
    {
        openpower::phal::sbe::collectPPEState(
            proc, dumpPath.string().c_str(), baseFilename.c_str());
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("collectPPEState failed: ") + e.what());
    }
}

void finalizeSbeCollection(targeting::TargetHandle pibFsiTarget,
                            const std::filesystem::path& dumpPath,
                            bool success,
                            [[maybe_unused]] int sbeTypeId)
{
    try
    {
        openpower::phal::sbe::finalizeCollection(
            pibFsiTarget, dumpPath.string().c_str(), success);
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("finalizeCollection failed: ") + e.what());
    }
}

} // namespace openpower::dump::phal::chipop

#endif // USE_PHAL_OLD

// Made with Bob

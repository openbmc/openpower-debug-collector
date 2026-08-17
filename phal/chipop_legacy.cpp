#include "chipop_iface.hpp"

#include <libphal.H>
#include <phal_exception.H>

#include <phosphor-logging/lg2.hpp>

extern "C"
{
#include <libpdbg.h>
}

namespace openpower::dump::phal::chipop
{
namespace
{

ChipOpError::Type classify(const openpower::phal::sbeError_t& error) noexcept
{
    using namespace openpower::phal::exception;

    switch (error.errType())
    {
        case SBE_CHIPOP_NOT_ALLOWED:
            return ChipOpError::Type::NotAllowed;
        case SBE_CMD_TIMEOUT:
            return ChipOpError::Type::Timeout;
        case SBE_FFDC_NO_DATA:
            return ChipOpError::Type::NoFfdc;
        case SBE_INTERNAL_FFDC_DATA:
            return ChipOpError::Type::InternalFfdc;
        default:
            return ChipOpError::Type::Failed;
    }
}

} // namespace

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
        return static_cast<uint32_t>(
            std::get<std::vector<uint8_t>>(storage_).size());
    }
    return 0;
}

// ============================================================================
// Chip Operations
// ============================================================================

DumpData getDump(targeting::TargetHandle chip, uint8_t dumpType,
                 uint8_t clockState, uint8_t collectFastArray)
{
    uint8_t* dumpPtr = nullptr;
    uint32_t dumpLen = 0;
    try
    {
        openpower::phal::sbe::getDump(chip, dumpType, clockState,
                                      collectFastArray, &dumpPtr, &dumpLen);

        return DumpData::fromMalloc(dumpPtr, dumpLen);
    }
    catch (openpower::phal::sbeError_t& e)
    {
        const auto errType = classify(e);
        std::string message = e.what();
        auto nativeError =
            std::make_shared<openpower::phal::sbeError_t>(std::move(e));
        std::shared_ptr<DumpData> partialData;
        if (dumpPtr != nullptr)
        {
            partialData = std::make_shared<DumpData>(
                DumpData::fromMalloc(dumpPtr, dumpLen));
        }
        throw ChipOpError(errType, std::move(message),
                          std::move(nativeError), std::move(partialData));
    }
}

void threadStopProc(targeting::TargetHandle proc)
{
    try
    {
        openpower::phal::sbe::threadStopProc(proc);
    }
    catch (openpower::phal::sbeError_t& e)
    {
        const auto errType = classify(e);
        std::string message = e.what();
        auto nativeError =
            std::make_shared<openpower::phal::sbeError_t>(std::move(e));
        throw ChipOpError(errType, std::move(message),
                          std::move(nativeError));
    }
}

// ============================================================================
// SBE Dump Collection HWPs
// ============================================================================

void initSbeCollection()
{
    // Initialize pdbg for SBE dump collection.
    // Uses the same init path as targeting_legacy.cpp::init().
    // The EKB library is loaded as part of pdbg init via libphal.
    openpower::phal::dump::initializePdbgLibEkb();
}

targeting::TargetHandle getTargetForSBEDump(uint32_t failingUnit, int sbeTypeId)
{
    return openpower::phal::dump::getTargetFromFailingId(failingUnit,
                                                          sbeTypeId);
}

targeting::TargetHandle probeSbeTarget(targeting::TargetHandle proc,
                                       const std::string& subTarget,
                                       int sbeTypeId)
{
    return openpower::phal::dump::probeTarget(proc, subTarget.c_str(),
                                               sbeTypeId);
}

void checkSbeState(targeting::TargetHandle pibFsiTarget,
                   int sbeTypeId)
{
    try
    {
        openpower::phal::dump::checkSbeState(pibFsiTarget, sbeTypeId);
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("SBE state check failed: ") + e.what());
    }
}

void sbeExtractRC(targeting::TargetHandle proc,
                  const std::filesystem::path& dumpPath, int sbeTypeId)
{
    openpower::phal::dump::executeSbeExtractRc(proc, dumpPath, sbeTypeId);
}

void collectLocalRegDump(targeting::TargetHandle proc,
                         const std::filesystem::path& dumpPath,
                         const std::string& baseFilename, int sbeTypeId)
{
    try
    {
        openpower::phal::dump::collectLocalRegDump(
            proc, dumpPath.string().c_str(), baseFilename.c_str(), sbeTypeId);
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("collectLocalRegDump failed: ") +
                              e.what());
    }
}

void collectPIBMSRegDump(targeting::TargetHandle proc,
                         const std::filesystem::path& dumpPath,
                         const std::string& baseFilename, int sbeTypeId)
{
    try
    {
        openpower::phal::dump::collectPIBMSRegDump(
            proc, dumpPath.string().c_str(), baseFilename.c_str(), sbeTypeId);
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("collectPIBMSRegDump failed: ") +
                              e.what());
    }
}

void collectPIBMEMDump(targeting::TargetHandle proc,
                       const std::filesystem::path& dumpPath,
                       const std::string& baseFilename, int sbeTypeId)
{
    try
    {
        openpower::phal::dump::collectPIBMEMDump(
            proc, dumpPath.string().c_str(), baseFilename.c_str(), sbeTypeId);
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("collectPIBMEMDump failed: ") + e.what());
    }
}

void collectPPEState(targeting::TargetHandle proc,
                     const std::filesystem::path& dumpPath,
                     const std::string& baseFilename, int sbeTypeId)
{
    try
    {
        openpower::phal::dump::collectPPEState(proc, dumpPath.string().c_str(),
                                               baseFilename.c_str(), sbeTypeId);
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("collectPPEState failed: ") + e.what());
    }
}

void finalizeSbeCollection(targeting::TargetHandle pibFsiTarget,
                           const std::filesystem::path& dumpPath, bool success,
                           int sbeTypeId)
{
    try
    {
        openpower::phal::dump::finalizeCollection(
            pibFsiTarget, dumpPath.string().c_str(), success, sbeTypeId);
    }
    catch (const openpower::phal::sbeError_t& e)
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          std::string("finalizeCollection failed: ") +
                              e.what());
    }
}

} // namespace openpower::dump::phal::chipop

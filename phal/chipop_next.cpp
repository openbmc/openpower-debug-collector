#include "chipop_iface.hpp"

#include <dump.H>

#include <phosphor-logging/lg2.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace openpower::dump::phal::chipop
{

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

DumpData getDump(targeting::TargetHandle chip, uint8_t dumpType,
                 uint8_t clockState, uint8_t collectFastArray)
{
    std::vector<sbei::ResponseInfo::FFDC> ffdc;
    std::vector<uint8_t> dumpData;

    // Call dump module wrapper - handles target conversion internally
    auto rc = hostfw::dump::getDump(chip, dumpType, clockState,
                                         collectFastArray, dumpData, ffdc);
    if (rc != 0)
    {
        std::string msg = "getDump failed with RC=" + std::to_string(rc);
        if (!ffdc.empty())
        {
            msg += ", FFDC count=" + std::to_string(ffdc.size());
        }
        throw ChipOpError(ChipOpError::Type::Failed, msg);
    }

    lg2::info("PHAL Next: getDump collected {SIZE} bytes", "SIZE",
              dumpData.size());
    return DumpData::fromVector(std::move(dumpData));
}

void threadStopProc(targeting::TargetHandle)
{
    // Not implemented
}

void initSbeCollection()
{
    // No-op: targeting already initialized by init()
}

targeting::TargetHandle getTargetForSBEDump(uint32_t, int)
{
    // Not used: targets enumerated via targeting_next.cpp
    throw std::runtime_error("getTargetForSBEDump not supported");
}

targeting::TargetHandle probeSbeTarget(targeting::TargetHandle,
                                       const std::string&, int)
{
    // Not used: no pdbg probe concept in next backend
    throw std::runtime_error("probeSbeTarget not supported");
}

void checkSbeState(targeting::TargetHandle, int)
{
    // Not implemented
}

void sbeExtractRC(targeting::TargetHandle, const std::filesystem::path&, int)
{
    // Not implemented
}

void collectLocalRegDump(targeting::TargetHandle, const std::filesystem::path&,
                         const std::string&, int)
{
    // Not used: collected via getDump() in next backend
    throw std::runtime_error("collectLocalRegDump not supported");
}

void collectPIBMSRegDump(targeting::TargetHandle, const std::filesystem::path&,
                         const std::string&, int)
{
    // Not used: collected via getDump() in next backend
    throw std::runtime_error("collectPIBMSRegDump not supported");
}

void collectPIBMEMDump(targeting::TargetHandle, const std::filesystem::path&,
                       const std::string&, int)
{
    // Not used: collected via getDump() in next backend
    throw std::runtime_error("collectPIBMEMDump not supported");
}

void collectPPEState(targeting::TargetHandle, const std::filesystem::path&,
                     const std::string&, int)
{
    // Not used: collected via getDump() in next backend
    throw std::runtime_error("collectPPEState not supported");
}

void finalizeSbeCollection(targeting::TargetHandle,
                           const std::filesystem::path&, bool, int)
{
    // Not implemented
}

} // namespace openpower::dump::phal::chipop

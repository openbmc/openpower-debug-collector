#include "chipop_iface.hpp"

#include <dump.H>
#include <errl_handle.H>
#include <ipl_facade.H>

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace openpower::dump::phal::chipop
{
namespace
{

std::optional<uint32_t> parseNumber(
    const std::unordered_map<std::string, std::string>& data,
    std::string_view key) noexcept
{
    const auto value = data.find(std::string(key));
    if (value == data.end())
    {
        return std::nullopt;
    }

    try
    {
        std::size_t parsed = 0;
        const auto number = std::stoul(value->second, &parsed, 0);
        if (parsed == value->second.size())
        {
            return static_cast<uint32_t>(number);
        }
    }
    catch (const std::exception&)
    {}
    return std::nullopt;
}

ChipOpError::Type classify(const errl::ErrlHandle& errors) noexcept
{
    constexpr uint32_t commandNotAllowed = 0x08;
    constexpr uint32_t notAllowedViaFifo1 = 0x4C;
    constexpr std::array<uint32_t, 5> timeoutStatuses = {
        0x10, // Hardware operation timeout
        0x21, // DML timeout
        0x28, // Special wakeup timeout
        0xC4, // Internal FIFO timeout
        0xF8, // Client SBE timeout
    };

    bool notAllowed = false;
    for (const auto& entry : errors.getEntries())
    {
        const auto& data = entry->getAdditionalData();
        if (const auto errorCode = parseNumber(data, "ERROR_CODE");
            errorCode &&
            (*errorCode == static_cast<uint32_t>(ETIMEDOUT) ||
             *errorCode == static_cast<uint32_t>(ETIME)))
        {
            return ChipOpError::Type::Timeout;
        }

        const auto status =
            parseNumber(data, "SBE_RESPONSE_SECONDARY_STATUS");
        if (!status)
        {
            continue;
        }
        if (*status == commandNotAllowed || *status == notAllowedViaFifo1)
        {
            notAllowed = true;
        }
        if (std::ranges::find(timeoutStatuses, *status) !=
            timeoutStatuses.end())
        {
            return ChipOpError::Type::Timeout;
        }
    }

    return notAllowed ? ChipOpError::Type::NotAllowed
                      : ChipOpError::Type::Failed;
}

} // namespace

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

DumpData getDump(targeting::TargetHandle chip, uint8_t dumpType,
                 uint8_t clockState, uint8_t collectFastArray)
{
    std::vector<uint8_t> dumpData;

    auto errors = hostfw::dump::getDump(chip, dumpType, clockState,
                                        collectFastArray, dumpData);
    if (errors)
    {
        const auto errorType = classify(*errors);
        auto nativeError =
            std::make_shared<errl::ErrlHandle>(std::move(*errors));
        throw ChipOpError(errorType, "HostFW getDump failed",
                          std::move(nativeError));
    }

    if (dumpData.empty())
    {
        throw ChipOpError(ChipOpError::Type::Failed,
                          "HostFW getDump returned no data");
    }

    lg2::info("PHAL Next: getDump collected {SIZE} bytes", "SIZE",
              dumpData.size());
    return DumpData::fromVector(std::move(dumpData));
}

void threadStopProc(targeting::TargetHandle proc)
{
    constexpr uint32_t allCores = 0x10290000;
    constexpr uint8_t ignoreHardwareErrors = 1;
    constexpr uint8_t allThreads = 0xF;
    constexpr uint8_t stopInstruction = 0x1;

    auto errors = hostfw::ipl::sppeControlInstructions(
        proc, allCores, ignoreHardwareErrors, allThreads, stopInstruction);
    if (errors)
    {
        const auto errorType = classify(*errors);
        auto nativeError =
            std::make_shared<errl::ErrlHandle>(std::move(*errors));
        throw ChipOpError(errorType,
                          "HostFW control-instructions failed",
                          std::move(nativeError));
    }
}

} // namespace openpower::dump::phal::chipop

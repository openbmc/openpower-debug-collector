#include "create_pel.hpp"
#include "dump_utils.hpp"
#include "error_iface.hpp"
#include "sbe_type.hpp"

#include <ekb/hwpf/fapi2/include/target_types.H>
#include <phal_exception.H>

#include <phosphor-logging/lg2.hpp>

#include <format>
#include <memory>

namespace openpower::dump::phal::error
{

std::vector<uint32_t> createChipOpErrorPELs(
    const chipop::ChipOpError& err, targeting::TargetHandle chip,
    const std::string& event, uint32_t cmdClass, uint32_t cmdType,
    [[maybe_unused]] const std::filesystem::path& dumpPath)
{
    auto native =
        std::static_pointer_cast<openpower::phal::sbeError_t>(err.native);
    if (!native)
    {
        lg2::error("Legacy chip-op error is missing its native SBE FFDC");
        return {};
    }

    const auto position = targeting::chipPos(chip);
    const auto chipType = targeting::getChipType(chip);
    const auto sbeType = chipType == targeting::ChipType::OcmbChip
                             ? SBETypes::OCMB
                             : SBETypes::PROC;

    pel::FFDCData additionalData = {
        {"SRC6", std::format("0x{:X}{:X}", position, (cmdClass | cmdType))}};
    if (sbeType == SBETypes::OCMB)
    {
        additionalData.emplace_back(
            "CHIP_TYPE", std::to_string(fapi2::TARGET_TYPE_OCMB_CHIP));
    }

    std::vector<uint32_t> logIds;
    try
    {
        if (err.type == chipop::ChipOpError::Type::Timeout ||
            err.type == chipop::ChipOpError::Type::NoFfdc)
        {
            auto logId = pel::createSbeErrorPEL(event, *native, additionalData);
            logIds.push_back(logId);

            if (err.type == chipop::ChipOpError::Type::Timeout)
            {
                const auto pelInfo = pel::getLogInfo(logId);
                util::requestSBEDump(position, std::get<0>(pelInfo), sbeType);
            }
            return logIds;
        }

        logIds = pel::processFFDCPackets(*native, event, additionalData);
        if (logIds.empty())
        {
            logIds.push_back(
                pel::createSbeErrorPEL(event, *native, additionalData));
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to create legacy chip-op PEL: {ERROR}", "ERROR",
                   e.what());
    }

    return logIds;
}

std::tuple<uint32_t, std::string> getPelInfo(uint32_t logId)
{
    return pel::getLogInfo(logId);
}

} // namespace openpower::dump::phal::error

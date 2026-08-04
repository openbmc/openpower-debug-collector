#include "targeting_iface.hpp"

#include <phosphor-logging/lg2.hpp>

#include <limits>
#include <stdexcept>

// HostFW targeting APIs used by the P11/PST backend.
#include <target_utils.H>

namespace openpower::dump::phal::targeting
{

void init()
{
    TARGETING::utils::targetingInit();
}

TargetList getPrimaryTargets()
{
    TargetList result;

    try
    {
        auto hubs = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);

        // Filter for functional targets only
        for (auto hub : hubs)
        {
            if (TARGETING::utils::isFunctional(hub))
            {
                result.push_back(hub);
            }
        }

        lg2::info("PHAL Next: found {COUNT} functional Hub targets", "COUNT",
                  result.size());
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: getPrimaryTargets failed: {ERROR}", "ERROR",
                   ex.what());
        throw;
    }

    return result;
}

TargetHandle findPrimaryTarget(uint32_t position)
{
    auto hubs = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    for (auto hub : hubs)
    {
        if (TARGETING::utils::getFapiPos(hub) == position)
        {
            return hub;
        }
    }
    return nullptr;
}

TargetList getAssociatedTargets(TargetHandle hub)
{
    TargetList result;

    if (hub == nullptr)
    {
        throw std::invalid_argument("getAssociatedTargets: null Hub target");
    }

    try
    {
        auto ocmbs = TARGETING::utils::getChildTargets(
            hub, TARGETING::TYPE_OCMB_CHIP, TARGETING::childByAffinity);
        for (auto ocmb : ocmbs)
        {
            if (TARGETING::utils::isFunctional(ocmb))
            {
                result.push_back(ocmb);
            }
        }

        lg2::info("PHAL Next: found {COUNT} functional OCMB targets", "COUNT",
                  result.size());
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: getAssociatedTargets failed: {ERROR}", "ERROR",
                   ex.what());
        throw;
    }

    return result;
}

bool isFunctional(TargetHandle target)
{
    if (target == nullptr)
    {
        return false;
    }

    try
    {
        return TARGETING::utils::isFunctional(target);
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: isFunctional failed: {ERROR}", "ERROR",
                   ex.what());
        return false;
    }
}

bool isUsable(TargetHandle target)
{
    // Phal-next has no pdbg probe state, so functional means usable here.
    return isFunctional(target);
}

uint32_t chipPos(TargetHandle target)
{
    if (target == nullptr)
    {
        throw std::invalid_argument("chipPos: null target");
    }

    try
    {
        // Get FAPI position (matches failingUnit semantics)
        auto position = TARGETING::utils::getFapiPos(target);
        if (position == std::numeric_limits<uint32_t>::max())
        {
            throw std::runtime_error("target has no ATTR_FAPI_POS");
        }
        return position;
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: chipPos failed: {ERROR}", "ERROR", ex.what());
        throw;
    }
}

uint32_t nodePos(TargetHandle target)
{
    if (target == nullptr)
    {
        throw std::invalid_argument("nodePos: null target");
    }

    auto node = TARGETING::utils::getParentTarget(target, TARGETING::TYPE_NODE);
    if (node == nullptr)
    {
        throw std::runtime_error("target has no TYPE_NODE parent");
    }

    auto position = TARGETING::utils::getFapiPos(node);
    if (position == std::numeric_limits<uint32_t>::max())
    {
        throw std::runtime_error("node has no ATTR_FAPI_POS");
    }
    return position;
}

ChipType getChipType(TargetHandle target)
{
    if (target == nullptr)
    {
        throw std::invalid_argument("getChipType: null target");
    }

    try
    {
        // Get target type attribute
        auto type = target->getAttr<TARGETING::ATTR_TYPE>();

        if (type == TARGETING::TYPE_HUB_CHIP)
        {
            return ChipType::HubChip;
        }
        if (type == TARGETING::TYPE_OCMB_CHIP)
        {
            return ChipType::OcmbChip;
        }

        throw std::runtime_error("unsupported phal-next dump target type");
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: getChipType failed: {ERROR}", "ERROR",
                   ex.what());
        throw;
    }
}

std::string getChipName(TargetHandle target)
{
    ChipType type = getChipType(target);
    if (type == ChipType::HubChip)
    {
        return "sock";
    }
    if (type == ChipType::OcmbChip)
    {
        return "ocmb";
    }
    throw std::runtime_error("unsupported phal-next dump target type");
}

std::string debugPath(TargetHandle target)
{
    if (target == nullptr)
    {
        return "<null>";
    }

    try
    {
        // Get physical path string for debugging
        return TARGETING::utils::getPhysicalPath(target);
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: debugPath failed: {ERROR}", "ERROR", ex.what());
        return "<unknown>";
    }
}

} // namespace openpower::dump::phal::targeting

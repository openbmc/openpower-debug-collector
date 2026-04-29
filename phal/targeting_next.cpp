#include "targeting_iface.hpp"

#include <phosphor-logging/lg2.hpp>

#include <stdexcept>

// Required headers for next-gen PHAL targeting
#include <hwaccess/hw_access_util.H>
#include <target_utils.H>
#include <targetsvc/target_service.H>

namespace openpower::dump::phal::targeting
{

void init()
{
    TARGETING::utils::targetingInit();
}

TargetList getAllProcTargets()
{
    TargetList result;

    try
    {
        // Get all processor targets (TYPE_HUB_CHIP in P11+)
        auto procList = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);

        // Filter for functional targets only
        for (auto proc : procList)
        {
            if (TARGETING::utils::isFunctional(proc))
            {
                result.push_back(proc);
            }
        }

        lg2::info("PHAL Next: found {COUNT} functional processor targets",
                  "COUNT", result.size());
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: getAllProcTargets failed: {ERROR}", "ERROR",
                   ex.what());
        throw;
    }

    return result;
}

TargetList getAllOCMBTargets(TargetHandle proc)
{
    TargetList result;

    if (proc == nullptr)
    {
        lg2::error("PHAL Next: getAllOCMBTargets called with null processor");
        return result;
    }

    try
    {
        // Use getChildTargets to get OCMB chips directly associated with this
        // processor This uses the affinity/association links, not physical
        // hierarchy
        auto ocmbList = TARGETING::utils::getChildTargets(
            proc, TARGETING::TYPE_OCMB_CHIP, TARGETING::childByAffinity);

        // Filter for functional targets only
        for (auto ocmb : ocmbList)
        {
            if (TARGETING::utils::isFunctional(ocmb))
            {
                result.push_back(ocmb);
            }
        }

        lg2::info("PHAL Next: found {COUNT} functional OCMB targets under proc",
                  "COUNT", result.size());
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: getAllOCMBTargets failed: {ERROR}", "ERROR",
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
    // For next-gen PHAL, usable == functional (no probe concept)
    return isFunctional(target);
}

uint32_t chipPos(TargetHandle target)
{
    if (target == nullptr)
    {
        return 0;
    }

    try
    {
        // Get FAPI position (matches failingUnit semantics)
        return TARGETING::utils::getPosition(target);
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: chipPos failed: {ERROR}", "ERROR", ex.what());
        return 0;
    }
}

ChipType getChipType(TargetHandle target)
{
    if (target == nullptr)
    {
        return ChipType::ProcChip;
    }

    try
    {
        // Get target type attribute
        auto type = target->getAttr<TARGETING::ATTR_TYPE>();

        // Classify based on type
        if (type == TARGETING::TYPE_OCMB_CHIP)
        {
            return ChipType::OcmbChip;
        }
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: getChipType failed: {ERROR}", "ERROR",
                   ex.what());
    }

    return ChipType::ProcChip;
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

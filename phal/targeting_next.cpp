#ifdef USE_PHAL_NEXT

#include "targeting_iface.hpp"
#include <phosphor-logging/lg2.hpp>
#include <stdexcept>

// Required headers for next-gen PHAL targeting
#include <targetsvc/target_service.H>
#include <target_utils.H>
#include <hwaccess/hw_access_util.H>

namespace openpower::dump::phal::targeting {

void init()
{
    try
    {
        // Initialize the targeting service with device tree path
        auto& ts = TARGETING::TargetService::instance();
        ts.init("/var/lib/phosphor-software-manager/pnor/rw/DEVTREE");
        
        // Set hardware access method for processor chips (SBEFIFO)
        hwaccess::utils::setHwAccessMethod(
            TARGETING::HW_ACCESS_METHOD_SBEFIFO,
            TARGETING::TYPE_HUB_CHIP
        );
        
        lg2::info("PHAL Next: targeting initialized successfully");
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: targeting init failed: {ERROR}", "ERROR", ex.what());
        throw;
    }
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
            if (proc != nullptr && TARGETING::utils::isFunctional(proc))
            {
                result.push_back(proc);
            }
        }
        
        lg2::info("PHAL Next: found {COUNT} functional processor targets",
                  "COUNT", result.size());
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: getAllProcTargets failed: {ERROR}", "ERROR", ex.what());
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
        // Get all OCMB chip targets under this processor
        auto ocmbList = TARGETING::utils::getChildTargets(proc, TARGETING::TYPE_OCMB_CHIP);
        
        // Filter for functional targets only
        for (auto ocmb : ocmbList)
        {
            if (ocmb != nullptr && TARGETING::utils::isFunctional(ocmb))
            {
                result.push_back(ocmb);
            }
        }
        
        lg2::info("PHAL Next: found {COUNT} functional OCMB targets under proc",
                  "COUNT", result.size());
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: getAllOCMBTargets failed: {ERROR}", "ERROR", ex.what());
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
        lg2::error("PHAL Next: isFunctional failed: {ERROR}", "ERROR", ex.what());
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
        return TARGETING::utils::getFapiPos(target);
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: chipPos failed: {ERROR}", "ERROR", ex.what());
        return 0;
    }
}

NodeKind kind(TargetHandle target)
{
    if (target == nullptr)
    {
        return NodeKind::ProcChip;
    }
    
    try
    {
        // Get target type attribute
        auto type = target->getAttr<TARGETING::ATTR_TYPE>();
        
        // Classify based on type
        if (type == TARGETING::TYPE_OCMB_CHIP)
        {
            return NodeKind::OcmbChip;
        }
        
        // Default to processor chip (TYPE_HUB_CHIP)
        return NodeKind::ProcChip;
    }
    catch (const std::exception& ex)
    {
        lg2::error("PHAL Next: kind failed: {ERROR}", "ERROR", ex.what());
        return NodeKind::ProcChip;
    }
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

#endif // USE_PHAL_NEXT

// Made with Bob

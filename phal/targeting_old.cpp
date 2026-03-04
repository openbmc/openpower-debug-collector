#ifdef USE_PHAL_OLD

#include "targeting_iface.hpp"
#include <libphal.H>
#include <phosphor-logging/lg2.hpp>

extern "C" {
#include <libpdbg.h>
#include <libpdbg_sbe.h>
}

namespace openpower::dump::phal::targeting {

void init()
{
    openpower::phal::pdbg::init();
}

TargetList getAllProcTargets()
{
    TargetList targets;
    struct pdbg_target* target = nullptr;
    
    pdbg_for_each_class_target("proc", target)
    {
        if (pdbg_target_probe(target) != PDBG_TARGET_ENABLED)
        {
            continue;
        }
        
        if (!openpower::phal::pdbg::isTgtFunctional(target))
        {
            continue;
        }
        
        targets.push_back(target);
    }
    
    return targets;
}

TargetList getAllOCMBTargets(TargetHandle proc)
{
    TargetList targets;
    struct pdbg_target* ocmbTarget = nullptr;
    
    pdbg_for_each_target("ocmb", proc, ocmbTarget)
    {
        // Filter for Odyssey OCMB chips only
        if (!is_ody_ocmb_chip(ocmbTarget))
        {
            continue;
        }
        
        if (pdbg_target_probe(ocmbTarget) != PDBG_TARGET_ENABLED)
        {
            continue;
        }
        
        if (!openpower::phal::pdbg::isTgtFunctional(ocmbTarget))
        {
            continue;
        }
        
        targets.push_back(ocmbTarget);
    }
    
    return targets;
}

bool isFunctional(TargetHandle target)
{
    return openpower::phal::pdbg::isTgtFunctional(target);
}

bool isUsable(TargetHandle target)
{
    return (pdbg_target_probe(target) == PDBG_TARGET_ENABLED) &&
           isFunctional(target);
}

uint32_t chipPos(TargetHandle target)
{
    return pdbg_target_index(target);
}

NodeKind kind(TargetHandle target)
{
    if (is_ody_ocmb_chip(target))
    {
        return NodeKind::OcmbChip;
    }
    return NodeKind::ProcChip;
}

std::string debugPath(TargetHandle target)
{
    const char* path = pdbg_target_path(target);
    return path ? std::string(path) : "<unknown>";
}

} // namespace openpower::dump::phal::targeting

#endif // USE_PHAL_OLD

// Made with Bob

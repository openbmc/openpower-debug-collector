#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Backend-specific includes
#ifdef USE_PHAL_NEXT
  #include <targeting/target.H>
#else
  extern "C" {
    #include <libpdbg.h>
  }
#endif

namespace openpower::dump::phal::targeting {

// ============================================================================
// Backend-Selected Target Handle
// ============================================================================

#ifdef USE_PHAL_NEXT
  using TargetHandle = TARGETING::TargetPtr;
#else
  using TargetHandle = struct pdbg_target*;
#endif

using TargetList = std::vector<TargetHandle>;

// ============================================================================
// Semantic Node Kinds
// ============================================================================

enum class NodeKind {
    ProcChip,   // Processor chip (owns SBEFIFO)
    OcmbChip    // Odyssey Memory Buffer chip
};

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief Initialize the targeting system
 * 
 * Old: openpower::phal::pdbg::init()
 * Next: TARGETING::utils::targetingInit()
 * 
 * @throws std::runtime_error on initialization failure
 */
void init();

// ============================================================================
// Target Enumeration
// ============================================================================

/**
 * @brief Get all functional processor targets
 * 
 * Old: pdbg_for_each_class_target("proc") + probe + functional check
 * Next: TARGETING::utils::getTargets(TYPE_HUB_CHIP) + functional filter
 * 
 * @return List of functional processor targets
 */
TargetList getAllProcTargets();

/**
 * @brief Get all functional OCMB targets under a processor
 * 
 * Old: pdbg_for_each_target("ocmb", proc) + is_ody_ocmb_chip + probe + functional
 * Next: TARGETING::utils::getChildTargets(proc, TYPE_OCMB_CHIP) + functional filter
 * 
 * @param proc The processor target
 * @return List of functional OCMB targets under the processor
 */
TargetList getAllOCMBTargets(TargetHandle proc);

// ============================================================================
// Target Properties
// ============================================================================

/**
 * @brief Check if target is functional
 * 
 * Old: openpower::phal::pdbg::isTgtFunctional()
 * Next: TARGETING::utils::isFunctional() (ATTR_HWAS_STATE.functional)
 * 
 * @param target Target handle
 * @return true if functional, false otherwise
 */
bool isFunctional(TargetHandle target);

/**
 * @brief Check if target is usable (functional + accessible)
 * 
 * Old: probe==ENABLED && isFunctional
 * Next: isFunctional (probe not applicable)
 * 
 * @param target Target handle
 * @return true if usable, false otherwise
 */
bool isUsable(TargetHandle target);

/**
 * @brief Get chip position (matches failingUnit semantics)
 * 
 * Old: pdbg_target_index()
 * Next: ATTR_FAPI_POS via utils::getFapiPos()
 * 
 * @param target Target handle
 * @return Chip position/index
 */
uint32_t chipPos(TargetHandle target);

/**
 * @brief Get target kind (semantic classification)
 * 
 * Old: is_ody_ocmb_chip() to classify
 * Next: ATTR_TYPE to distinguish TYPE_OCMB_CHIP vs TYPE_HUB_CHIP
 * 
 * @param target Target handle
 * @return NodeKind classification
 */
NodeKind kind(TargetHandle target);

/**
 * @brief Get debug path string for logging
 * 
 * Old: pdbg_target_path()
 * Next: TARGETING::utils::getPhysicalPath()
 * 
 * @param target Target handle
 * @return Debug path string
 */
std::string debugPath(TargetHandle target);

} // namespace openpower::dump::phal::targeting

// Made with Bob

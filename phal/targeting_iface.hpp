#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Backend-specific includes
#ifdef NEXT_PHAL
#include <targeting/target.H>
#else
extern "C"
{
#include <libpdbg.h>
}
#endif

namespace openpower::dump::phal::targeting
{

// ============================================================================
// Backend-Selected Target Handle
// ============================================================================

#ifdef NEXT_PHAL
using TargetHandle = TARGETING::ConstTargetPtr;
#else
using TargetHandle = struct pdbg_target*;
#endif

using TargetList = std::vector<TargetHandle>;

// ============================================================================
// Semantic Chip Types
// ============================================================================

enum class ChipType
{
    ProcChip,
    OcmbChip,
    HubChip,
};

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief Initialize the targeting system
 *
 * Legacy: openpower::phal::pdbg::init()
 * Next: TARGETING::utils::targetingInit()
 *
 * @throws std::runtime_error on initialization failure
 */
void init();

// ============================================================================
// Target Enumeration
// ============================================================================

/**
 * @brief Get the functional roots used for parallel dump collection
 *
 * Legacy: P10 processor targets.
 * Next: P11/PST Hub chip targets.
 *
 * @return List of functional primary targets
 */
TargetList getPrimaryTargets();

#ifdef NEXT_PHAL
/**
 * @brief Find a Hub target by its failing-unit position
 *
 * Unlike getPrimaryTargets(), this lookup includes non-functional targets so
 * an error can still be associated with the chip that requires recovery.
 *
 * @param position Chip position reported in the dump request
 * @return Matching Hub target, or nullptr when not found
 */
TargetHandle findPrimaryTarget(uint32_t position);
#endif

/**
 * @brief Get functional chips collected after a primary target
 *
 * Legacy: Odyssey OCMB targets under the P10 processor.
 * Next: functional OCMB chips associated with the Hub.
 *
 * @param primary Processor or Hub target
 * @return List of functional associated targets
 */
TargetList getAssociatedTargets(TargetHandle primary);

// ============================================================================
// Target Properties
// ============================================================================

/**
 * @brief Check if target is functional
 *
 * Legacy: openpower::phal::pdbg::isTgtFunctional()
 * Next: TARGETING::utils::isFunctional() (ATTR_HWAS_STATE.functional)
 *
 * @param target Target handle
 * @return true if functional, false otherwise
 */
bool isFunctional(TargetHandle target);

/**
 * @brief Check if target is usable (functional + accessible)
 *
 * Legacy: probe==ENABLED && isFunctional
 * Next: isFunctional (probe not applicable)
 *
 * @param target Target handle
 * @return true if usable, false otherwise
 */
bool isUsable(TargetHandle target);

/**
 * @brief Get chip position (matches failingUnit semantics)
 *
 * Legacy: pdbg_target_index()
 * Next: ATTR_FAPI_POS via utils::getFapiPos().
 *
 * @param target Target handle
 * @return Chip position/index
 */
uint32_t chipPos(TargetHandle target);

/**
 * @brief Get the node FAPI position for dump file naming
 *
 * Legacy returns zero to preserve the P10 filename contract.  The phal-next
 * backend resolves the target's TYPE_NODE parent.
 */
uint32_t nodePos(TargetHandle target);

/**
 * @brief Get target chip type (semantic classification)
 *
 * Legacy distinguishes P10 processor and Odyssey OCMB chips.
 * Next distinguishes P11/PST Hub and OCMB chips.
 *
 * @param target Target handle
 * @return ChipType classification
 */
ChipType getChipType(TargetHandle target);

/**
 * @brief Get chip name string for dump file naming
 *
 * Returns backend-specific chip name for use in dump file names and logging.
 *
 * Legacy (P10): "proc" or "ocmb".
 * Next (P11/PST): "sock" or "ocmb".
 *
 * @param target Target handle
 * @return Stable chip name used in the dump filename
 */
std::string getChipName(TargetHandle target);

/**
 * @brief Get debug path string for logging
 *
 * Legacy: pdbg_target_path()
 * Next: TARGETING::utils::getPhysicalPath()
 *
 * @param target Target handle
 * @return Debug path string
 */
std::string debugPath(TargetHandle target);

} // namespace openpower::dump::phal::targeting

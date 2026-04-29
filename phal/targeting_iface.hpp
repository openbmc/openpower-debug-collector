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
using TargetHandle = TARGETING::TargetPtr;
#else
using TargetHandle = struct pdbg_target*;
#endif

using TargetList = std::vector<TargetHandle>;

// ============================================================================
// Semantic Chip Types
// ============================================================================

enum class ChipType
{
    ProcChip, // Processor chip (owns SBEFIFO)
    OcmbChip  // Odyssey Memory Buffer chip
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
 * @brief Get all functional processor targets
 *
 * Legacy: pdbg_for_each_class_target("proc") + probe + functional check
 * Next: TARGETING::utils::getTargets(TYPE_HUB_CHIP) + functional filter
 *
 * @return List of functional processor targets
 */
TargetList getAllProcTargets();

/**
 * @brief Get all functional OCMB targets under a processor
 *
 * Legacy: pdbg_for_each_target("ocmb", proc) + is_ody_ocmb_chip + probe +
 * functional Next: TARGETING::utils::getChildTargets(proc, TYPE_OCMB_CHIP) +
 * functional filter
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
 * Next: ATTR_FAPI_POS via utils::getFapiPos()
 *
 * @param target Target handle
 * @return Chip position/index
 */
uint32_t chipPos(TargetHandle target);

/**
 * @brief Get target chip type (semantic classification)
 *
 * Legacy: is_ody_ocmb_chip() to classify
 * Next: ATTR_TYPE to distinguish TYPE_OCMB_CHIP vs TYPE_HUB_CHIP
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
 * Legacy (P10): "proc" for processors, "ocmb" for memory buffers
 * Next (P12):   "sock" for processors, "ocmb" for memory buffers
 *
 * @param target Target handle
 * @return Chip name string ("proc", "sock", or "ocmb")
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

#pragma once

#include "chipop_iface.hpp"
#include "targeting_iface.hpp"
#include <cstdint>
#include <filesystem>
#include <string>

namespace openpower::dump::phal::error {

/**
 * @brief Log chip-op error and create PEL
 *
 * Old: Parses sbeError_t FFDC and creates detailed PEL
 * Next: Logs error with basic info (Phase 1), full PEL support in Phase 2
 *
 * @param err The chip-op error
 * @param chip Target where error occurred
 * @param cmdClass Command class (for PEL)
 * @param cmdType Command type (for PEL)
 * @param dumpPath Dump collection path
 * @return true if error is recoverable (continue dump collection)
 *         false if error is critical (exclude target from collection)
 */
bool logChipOpError(const chipop::ChipOpError& err,
                    targeting::TargetHandle chip,
                    uint32_t cmdClass,
                    uint32_t cmdType,
                    const std::filesystem::path& dumpPath);

/**
 * @brief Create a PEL for a chip-op error and return the PEL/log ID
 *
 * Old: Calls D-Bus CreatePELWithFFDCFiles via phosphor-logging
 * Next: Logs via lg2 only (Phase 1 stub), returns 0
 *
 * @param err       The chip-op error
 * @param chip      Target where error occurred
 * @param event     D-Bus event name (e.g. sbeTypeAttributes.chipOpFailure)
 * @param dumpPath  Dump collection path (for FFDC context)
 * @return uint32_t  D-Bus log entry ID (0 if not created)
 */
uint32_t createChipOpErrorPEL(const chipop::ChipOpError& err,
                               targeting::TargetHandle chip,
                               const std::string& event,
                               const std::filesystem::path& dumpPath);

} // namespace openpower::dump::phal::error

// Made with Bob

#pragma once

#include "chipop_iface.hpp"
#include "targeting_iface.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <tuple>

namespace openpower::dump::phal::error
{

/**
 * @brief Log chip-op error and create PEL
 *
 * Legacy: Parses sbeError_t FFDC and creates detailed PEL
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
                    targeting::TargetHandle chip, uint32_t cmdClass,
                    uint32_t cmdType, const std::filesystem::path& dumpPath);

/**
 * @brief Create a PEL for a chip-op error and return the PEL/log ID
 *
 * Legacy: Calls D-Bus CreatePELWithFFDCFiles via phosphor-logging
 * Next: Logs via lg2 only (Phase 1 stub), returns 0
 *
 * @param err       The chip-op error
 * @param chip      Target where error occurred
 * @param event     D-Bus event name (e.g. sbeTypeAttributes.chipOpFailure)
 * @param dumpPath  Dump collection path (for FFDC context)
 * @return uint32_t  D-Bus log entry ID (0 if not created)
 */
uint32_t createChipOpErrorPEL(
    const chipop::ChipOpError& err, targeting::TargetHandle chip,
    const std::string& event, const std::filesystem::path& dumpPath);

/**
 * @brief Get PEL information from log ID
 *
 * Legacy: Retrieves PEL ID and SRC from D-Bus logging
 * Next: Stub (returns {0, ""})
 *
 * @param logId The log ID returned from createChipOpErrorPEL
 * @return tuple of (PEL ID, SRC string)
 */
std::tuple<uint32_t, std::string> getPelInfo(uint32_t logId);

} // namespace openpower::dump::phal::error

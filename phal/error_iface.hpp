#pragma once

#include "chipop_iface.hpp"
#include "targeting_iface.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

namespace openpower::dump::phal::error
{

/**
 * @brief Create PELs for a backend-native chip-op error
 *
 * Legacy preserves libphal SBE FFDC packet handling and timeout-triggered SBE
 * dumps.  Phal-next commits every HostFW ErrlEntry and its FFDC files.
 *
 * @param err       The chip-op error
 * @param chip      Target where error occurred
 * @param event     D-Bus event name (e.g. sbeTypeAttributes.chipOpFailure)
 * @param cmdClass  SBE FIFO command class
 * @param cmdType   SBE FIFO command identifier
 * @param dumpPath  Dump collection path (for FFDC context)
 * @return D-Bus logging entry IDs for the PELs that were created
 */
std::vector<uint32_t> createChipOpErrorPELs(
    const chipop::ChipOpError& err, targeting::TargetHandle chip,
    const std::string& event, uint32_t cmdClass, uint32_t cmdType,
    const std::filesystem::path& dumpPath);

/**
 * @brief Get PEL information from log ID
 *
 * Legacy: Retrieves PEL ID and SRC from D-Bus logging
 * Next: Retrieves PEL ID and SRC from D-Bus logging
 *
 * @param logId A logging entry ID returned by createChipOpErrorPELs
 * @return tuple of (PEL ID, SRC string)
 */
std::tuple<uint32_t, std::string> getPelInfo(uint32_t logId);

} // namespace openpower::dump::phal::error

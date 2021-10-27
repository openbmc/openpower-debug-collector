#pragma once

#include "dump_utils.hpp"

#include <filesystem>

namespace openpower
{
namespace dump
{

namespace sbe_chipop
{
/** @brief The function to write dump content to file
 *  @param path - Path to dump file
 *  @param id - A unique id assigned to dump to be collected
 *  @clockState - Clock state, ON or Off
 *  @param chipPos - Chip position of the failing unit
 *  @param dataPtr - Content to write to file
 *  @param len - Length of the content
 */
void writeDumpFile(const std::filesystem::path& path, const uint32_t id,
                   const uint8_t clockState, const uint8_t chipPos,
                   util::DumpDataPtr& dataPtr, const uint32_t len);

/** @brief The function to orchestrate dump collection from different
 *  SBEs
 *  @param type - Type of the dump
 *  @param id - A unique id assigned to dump to be collected
 *  @param failingUnit - Chip position of the failing unit
 *  @param sbeFilePath - Path where the collected dump to be stored
 */
void collectDump(const uint8_t type, const uint32_t id,
                 const uint64_t failingUnit, const std::filesystem::path& path);

/** @brief The function to collect dump from SBE
 *  @param[in] proc - pdbg_target of the proc containing SBE to collect the
 * dump.
 *  @param[in] dumpPath - Path of directory to write the dump files.
 *  @param[in] id - Id of the dump
 *  @param[in] type - Type of the dump
 *  @param[in] clockState - State of the clock while collecting.
 *  @param[in] chipPos - Position of the chip
 *  @param[in] failingUnit - Chip position of the failing unit
 */
void collectDumpFromSBE(struct pdbg_target* proc,
                        const std::filesystem::path& path, const uint32_t id,
                        const uint8_t type, const uint8_t clockState,
                        const uint8_t chipPos, const uint64_t failingUnit);

} // namespace sbe_chipop
} // namespace dump
} // namespace openpower

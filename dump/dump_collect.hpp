#pragma once

#include <filesystem>

namespace openpower
{
namespace dump
{

namespace sbe_chipop
{
/** @brief The function to orchestrate dump collection from different
 *  SBEs
 *  @param type - Type of the dump
 *  @param id - A unique id assigned to dump to be collected
 *  @param failingUnit - Chip position of the failing unit
 *  @param sbeFilePath - Path where the collected dump to be stored
 */
void collectDump(const uint8_t type, const uint32_t id,
                 const uint64_t failingUnit, const std::filesystem::path& path);

} // namespace sbe_chipop
} // namespace dump
} // namespace openpower

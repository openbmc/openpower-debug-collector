#pragma once

#include <libpdbg.h>

#include <filesystem>

namespace openpower
{
namespace dump
{

/** @struct DumpPtr
 * @brief a structure holding the data pointer
 * @details This is a RAII container for the dump data
 * returned by the SBE
 */
struct DumpDataPtr
{
  public:
    /** @brief Destructor for the object, free the allocated memory.
     */
    ~DumpDataPtr()
    {
        // The memory is allocated using malloc
        free(dataPtr);
    }

    /** @brief Returns the pointer to the data
     */
    uint8_t** getPtr()
    {
        return &dataPtr;
    }

    /** @brief Returns the stored data
     */
    uint8_t* getData()
    {
        return dataPtr;
    }

  private:
    /** The pointer to the data */
    uint8_t* dataPtr = NULL;
};

namespace sbe_chipop
{
/** @brief The function to orchestrate dump collection from different
 *  SBEs
 *  @param type - Type of the dump
 *  @param id - A unique id assigned to dump to be collected
 *  @param failingUnit - Chip position of the failing unit
 *  @param sbeFilePath - Path where the collected dump to be stored
 */
void collectDump(uint8_t type, uint32_t id, const uint64_t failingUnit,
                 std::filesystem::path& sbeFilePath);

/** @brief The function to collect dump from SBE
 *  @param[in] proc - pdbg_target of the proc conating SBE to collect the
 * dump.
 *  @param[in] dumpPath - Path of directory to write the dump files.
 *  @param[in] id - Id of the dump
 *  @param[in] type - Type of the dump
 *  @param[in] clockState - State of the clock while collecting.
 *  @param[in] chipPos - Position of the chip
 *  @param[in] collectFastArray - 0: skip fast array collection 1: collect
 * fast array
 */
void collectDumpFromSBE(struct pdbg_target* proc,
                        std::filesystem::path& dumpPath, uint32_t id,
                        uint8_t type, uint8_t clockState, const uint8_t chipPos,
                        const uint8_t collectFastArray);
} // namespace sbe_chipop

namespace hwp
{
/** @brief The function to collect SBE dump
 *  @param[in] dumpPath - Path to store the dump
 *  @param[in] id - Id of the dump
 *  @param[in] chipPos - Position of the chip contains the SBE to be dumped.
 *  @param sbeFilePath - Path where the collected dump to be stored
 */
void collectDump(uint32_t id, const uint64_t failingUnit,
                 std::filesystem::path& sbeFilePath);
} // namespace hwp

/** @brief Method to find whether a processor is master
 *  @param[in] proc - pdbg_target for processor target
 *
 *  @return bool - true if master processor else false.
 */
bool isMasterProc(struct pdbg_target* proc);

/** @brief create dump directories and add error log id
 *  @param dumpPath Directory for collecting dump
 *  @param collectionPath sub directory to store files
 *  @errorLogId Error log id associated with dump
 */
void prepareCollection(const std::filesystem::path dumpPath,
                       std::string collectionPath, std::string errorLogId);

} // namespace dump
} // namespace openpower

#pragma once

extern "C"
{
#include <libpdbg.h>
#include <libpdbg_sbe.h>
}

#include "dump_utils.hpp"
#include "sbe_consts.hpp"
#include "sbe_type.hpp"

#include <phal_exception.H>

#include <cstdint>
#include <filesystem>
#include <future>
#include <vector>

namespace openpower::dump::sbe_chipop
{

/**
 * @class SbeDumpCollector
 * @brief Manages the collection of dumps from SBEs on failure.
 *
 * This class provides functionalities to orchestrate the collection of
 * diagnostic dumps from Self Boot Engines across multiple processors
 * in response to failures or for diagnostic purposes.
 */
class SbeDumpCollector
{
  public:
    /**
     * @brief Constructs a new SbeDumpCollector object.
     */
    SbeDumpCollector() = default;

    /**
     * @brief Destroys the SbeDumpCollector object.
     */
    ~SbeDumpCollector() = default;

    /**
     * @brief Orchestrates the collection of dumps from all available SBEs.
     *
     * Initiates the process of collecting diagnostic dumps from SBEs. This
     * involves identifying available processors, initiating the dump
     * collection process, and managing the collected dump files.
     *
     * @param type The type of dump to collect.
     * @param id A unique identifier for the dump collection operation.
     * @param failingUnit The identifier of the failing unit prompting the dump
     * collection.
     * @param path The filesystem path where collected dumps should be stored.
     */
    void collectDump(uint8_t type, uint32_t id, uint64_t failingUnit,
                     const std::filesystem::path& path);

  private:
    /**
     * @brief Collects a dump from a single SBE.
     *
     * Executes the low-level operations required to collect a diagnostic
     * dump from the specified SBE.
     *
     * @param chip A pointer to the pdbg_target structure representing the SBE.
     * @param path The filesystem path where the dump should be stored.
     * @param id The unique identifier for this dump collection operation.
     * @param type The type of dump to collect.
     * @param clockState The clock state of the SBE during dump collection.
     * @param failingUnit The identifier of the failing unit.
     */
    void collectDumpFromSBE(struct pdbg_target* chip,
                            const std::filesystem::path& path, uint32_t id,
                            uint8_t type, uint8_t clockState,
                            uint64_t failingUnit);

    /**
     * @brief Initializes the PDBG library.
     *
     * Prepares the PDBG library for interacting with processor targets. This
     * must be called before any PDBG-related operations are performed.
     */
    void initializePdbg();

    /**
     * @brief Launches asynchronous dump collection tasks for a set of targets.
     *
     * This method initiates the dump collection process asynchronously for each
     * target provided in the `targets` vector. It launches a separate
     * asynchronous task for each target, where each task calls
     * `collectDumpFromSBE` with the specified parameters, including the clock
     * state.
     *
     * @param type The type of the dump to collect. This could be a hardware
     * dump, software dump, etc., as defined by the SBE dump type enumeration.
     * @param id A unique identifier for the dump collection operation. This ID
     * is used to tag the collected dump for identification.
     * @param path The filesystem path where the collected dumps should be
     * stored. Each dump file will be stored under this directory.
     * @param failingUnit The identifier of the unit or component that is
     * failing or suspected to be the cause of the issue prompting the dump
     * collection. This is used for diagnostic purposes.
     * @param cstate The clock state during the dump collection. This parameter
     *               dictates whether the dump should be collected with the
     * clocks running (SBE_CLOCK_ON) or with the clocks stopped (SBE_CLOCK_OFF).
     * @param targets A vector of `pdbg_target*` representing the targets from
     * which dumps should be collected. Each target corresponds to a physical or
     * logical component in the system, such as a processor or an SBE.
     *
     * @return A vector of `std::future<void>` objects. Each future represents
     * the completion state of an asynchronous dump collection task. The caller
     *         can wait on these futures to determine when all dump collection
     * tasks have completed. Exceptions thrown by the asynchronous tasks are
     * captured by the futures and can be rethrown when the futures are
     * accessed.
     */
    std::vector<std::future<void>> spawnDumpCollectionProcesses(
        uint8_t type, uint32_t id, const std::filesystem::path& path,
        uint64_t failingUnit, uint8_t cstate,
        const std::vector<struct pdbg_target*>& targets);

    /** @brief This function creates the new dump file in dump file name
     * format and then writes the contents into it.
     *  @param path - Path to dump file
     *  @param id - A unique id assigned to dump to be collected
     *  @param clockState - Clock state, ON or Off
     *  @param nodeNum - Node containing the chip
     *  @param chipName - Name of the chip
     *  @param chipPos - Chip position of the failing unit
     *  @param dataPtr - Content to write to file
     *  @param len - Length of the content
     */
    void writeDumpFile(const std::filesystem::path& path, const uint32_t id,
                       const uint8_t clockState, const uint8_t nodeNum,
                       const std::string& chipName, const uint8_t chipPos,
                       util::DumpDataPtr& dataPtr, const uint32_t len);

    /**
     * @brief Determines if fastarray collection is needed based on dump type
     * and unit.
     *
     * @param clockState The current state of the clock.
     * @param type The type of the dump being collected.
     * @param failingUnit The ID of the failing unit.
     * @param chipPos The position of the chip for which the dump is being
     * collected.
     *
     * @return uint8_t - Returns 1 if fastarray collection is needed, 0
     * otherwise.
     */
    inline uint8_t checkFastarrayCollectionNeeded(const uint8_t clockState,
                                                  const uint8_t type,
                                                  uint64_t failingUnit,
                                                  const uint8_t chipPos) const
    {
        using namespace openpower::dump::SBE;

        return (clockState == SBE_CLOCK_OFF &&
                (type == SBE_DUMP_TYPE_HOSTBOOT ||
                 (type == SBE_DUMP_TYPE_HARDWARE && chipPos == failingUnit)))
                   ? 1
                   : 0;
    }

    /**
     * Logs an error and creates a PEL for SBE chip-op failures.
     *
     * @param sbeError - An error object encapsulating details about the SBE
     * error.
     * @param chipPos - The position of the chip where the error occurred.
     * @param sbeType - The type of SBE, used to determine the event log
     * message.
     * @param cmdClass - The command class associated with the SBE operation.
     * @param cmdType - The specific type of command within the command class.
     *
     */
    void logErrorAndCreatePEL(const openpower::phal::sbeError_t& sbeError,
                              uint64_t chipPos, SBETypes sbeType,
                              uint32_t cmdClass, uint32_t cmdType);

    /**
     * Determines the type of SBE for a given chip target.
     *
     * @param chip - A pointer to a pdbg_target structure representing the chip.
     * @return The SBE type for the given chip target.
     */
    inline SBETypes getSBEType([[maybe_unused]] struct pdbg_target* chip)
    {
        return SBETypes::PROC;
    }

    /**
     * @brief Executes thread stop on a processor target
     *
     * If the Self Boot Engine (SBE) is not ready to accept chip operations
     * (chip-ops), it logs the condition and excludes the processor from the
     * dump collection process. For critical errors, such as a timeout during
     * the stop operation, it logs the error and again excludes the processor.
     * In case of SBE command failure or non-critical errors, it continues with
     * the dump collection process.
     *
     * @param target Pointer to the pdbg target structure representing the
     *               processor to perform the thread stop on.
     * @return true If the thread stop was successful or in case of non-critical
     *              errors where dump collection can proceed.
     * @return false If the SBE is not ready for chip-ops or in case of critical
     *               errors like timeouts, indicating the processor should be
     *               excluded from the dump collection.
     */
    bool executeThreadStop(struct pdbg_target* target);
};

} // namespace openpower::dump::sbe_chipop

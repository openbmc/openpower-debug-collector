#pragma once
#include <stdint.h>

/**
 * @brief Main function to initiate Hostboot dump
 *
 */

namespace watchdog
{
namespace dump
{

/**
 * @brief Initiate Hostboot dump collection
 *
 * @param timeout - timeout interval in seconds
 */
void triggerHostbootDump(const uint32_t timeout);

/**
 * @brief Handle SBE Boot Error
 *
 * @param procTarget - Processor target
 * @param timeout - timeout interval in seconds
 */
void handleSbeBootError(struct pdbg_target* procTarget, const uint32_t timeout);

/**
 * @brief creates a PEL and triggers System dump
 *
 * @details This function creates the PEL and then triggers MPIPL
 *
 */
void triggerSystemDump();

} // namespace dump
} // namespace watchdog

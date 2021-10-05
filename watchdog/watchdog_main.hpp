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
 * @brief Initiate SBE dump collection
 *
 * @param procTarget - Processor target
 * @param timeout - timeout interval in seconds
 */
void triggerSBEDump(struct pdbg_target* procTarget, const uint32_t timeout);

} // namespace dump
} // namespace watchdog

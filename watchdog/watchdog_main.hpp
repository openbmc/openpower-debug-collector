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
 * @brief Initiate captureFFDC, and SBE dump if needed
 *
 * @param procTarget - Processor target
 */
void triggerCaptureFFDC(struct pdbg_target* procTarget);

} // namespace dump
} // namespace watchdog

#pragma once

#include <stdint.h>

/**
 * @brief Hostboot dump collector handler
 *
 * Handle collection due to host going down
 */

namespace watchdog
{
namespace dump
{
/**
 * Request a dump from the dump manager
 *
 * Request a dump from the dump manager and register a monitor for observing
 * the dump progress.
 *
 * @param logId The id of the event log associated with this dump request
 *
 */
void requestDump(const uint32_t logId);
} // namespace dump
} // namespace watchdog

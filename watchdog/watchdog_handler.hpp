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
 * @brief Request a dump from the dump manager
 *
 * Request a dump from the dump manager and register a monitor for observing
 * the dump progress.
 *
 * @param logId - the id of the event log associated with this dump request
 * @param timeout - timeout interval in seconds
 *
 */
void requestDump(const uint32_t logId, const uint32_t timeout);

} // namespace dump
} // namespace watchdog

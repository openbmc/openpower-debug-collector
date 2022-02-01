#pragma once
#include <stdint.h>

#include <nlohmann/json.hpp>

/**
 * @brief Main function to initiate Hostboot dump
 *
 */

namespace watchdog
{
namespace dump
{
using json = nlohmann::json;

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

} // namespace dump
} // namespace watchdog

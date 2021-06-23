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
 * Initiate Hostboot dump collection
 *
 */
void triggerHostbootDump();
} // namespace dump
} // namespace watchdog

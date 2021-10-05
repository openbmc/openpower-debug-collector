#pragma once

#include <stdint.h>

/** @brief Dump progress states */
enum class DumpProgressStatus
{
    InProgress,
    Completed,
    Failed
};

/**
 * @brief Hostboot dump collector handler
 *
 * Handle collection due to host going down
 */

namespace watchdog
{
namespace dump
{

/** @brief Dump types supported by dump request */
enum class DumpType
{
    Hostboot,
    SBE
};

/** @brief Structure for dump request parameters */
struct DumpParameters
{
    uint32_t logId;
    uint32_t unitId;
    uint32_t timeout;
    DumpType dumpType;
};

/**
 * @brief Request a dump from the dump manager
 *
 * Request a dump from the dump manager and register a monitor for observing
 * the dump progress.
 *
 * @param dumpParameters - parameters for the dump request
 *
 */
void requestDump(const DumpParameters&);

} // namespace dump
} // namespace watchdog

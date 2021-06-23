#pragma once

#include <cstddef> // for size_t
#include <map>
#include <string>
#include <vector>

namespace dump
{
namespace hbdump
{

constexpr int maxTraceLen = 64; // characters

constexpr auto pathLogging = "/xyz/openbmc_project/logging";
constexpr auto levelPelError = "xyz.openbmc_project.Logging.Entry.Level.Error";

/** @brief Logging level types */
enum level
{
    INFO,
    ERROR
};

/** @brief Create trace message template */
template <level L>
void trace(const char* i_message);

/** @brief Commit watchdog timeout handler failure event to log */
void eventWatchdogTimeout();

} // namespace hbdump
} // namespace dump

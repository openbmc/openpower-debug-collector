#include <unistd.h>

#include <phosphor-logging/log.hpp>
#include <watchdog_dbus.hpp>
#include <watchdog_handler.hpp>
#include <watchdog_logging.hpp>

namespace watchdog
{
namespace dump
{

/** @brief Journal entry of type INFO using phosphor logging */
template <>
void trace<INFO>(const char* i_message)
{
    phosphor::logging::log<phosphor::logging::level::INFO>(i_message);
}

template <>
void trace<ERROR>(const char* i_message)
{
    phosphor::logging::log<phosphor::logging::level::ERR>(i_message);
}

/**
 * Log an event handled by the dump handler
 *
 * @param  i_additional - Additional PEL data
 */
void event(std::map<std::string, std::string>& i_additional)
{

    std::string eventName = "org.open_power.Host.Boot.Error.WatchdogTimeout";

    // CreatePELWithFFDCFiles requires vector of FFDCTuple.
    // Need this empty vector
    auto emptyFfdc = std::vector<FFDCTuple>{};

    // Create PEL with additional data.
    auto pelId = createPel(eventName, i_additional, emptyFfdc);

    requestDump(pelId); // will not return until dump is complete
}

/**
 * Commit watchdog_timeout handler failure event to log
 *
 */
void eventWatchdogTimeout()
{
    // Additional data for log
    // Need this if we need to log additional data like
    // ERROR code.
    // Keeping this for now in case if we have to add
    // any data corresponding to watchdog timeout
    std::map<std::string, std::string> additionalData;
    event(additionalData);
}

} // namespace dump
} // namespace watchdog

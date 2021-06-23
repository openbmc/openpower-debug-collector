#include <unistd.h>

#include <watchdog_dbus.hpp>
#include <watchdog_handler.hpp>
#include <watchdog_logging.hpp>

namespace watchdog
{
namespace dump
{

/**
 * @brief Log an event handled by the dump handler
 *
 * @param  additional - Additional PEL data
 */
void event(std::map<std::string, std::string>& additional)
{

    std::string eventName = "org.open_power.Host.Boot.Error.WatchdogTimeout";

    // CreatePELWithFFDCFiles requires a vector of FFDCTuple.
    auto emptyFfdc = std::vector<FFDCTuple>{};

    // Create PEL with additional data.
    auto pelId = createPel(eventName, additional, emptyFfdc);

    requestDump(pelId); // will not return until dump is complete
}

void eventWatchdogTimeout()
{
    // Additional data to be added to PEL object
    // Currently we don't have anything to add
    // Keeping this for now in case if we have to add
    // any data corresponding to watchdog timeout
    std::map<std::string, std::string> additionalData;
    event(additionalData);
}

} // namespace dump
} // namespace watchdog

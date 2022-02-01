#include <unistd.h>

#include <phosphor-logging/log.hpp>
#include <watchdog_common.hpp>
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
 * @param additional - Additional PEL data
 * @param timeout - timeout interval in seconds
 */
void event(std::map<std::string, std::string>& additional,
           const uint32_t timeout)
{

    std::string eventName = "org.open_power.Host.Boot.Error.WatchdogTimeout";

    // CreatePELWithFFDCFiles requires a vector of FFDCTuple.
    auto emptyFfdc = std::vector<FFDCTuple>{};

    // Create PEL with additional data.
    auto pelId = createPel(eventName, additional, emptyFfdc);

    // Collect Hostboot dump if auto reboot is enabled
    DumpParameters dumpParameters;
    dumpParameters.logId = pelId;
    dumpParameters.unitId = 0; // Not used for Hostboot dump
    dumpParameters.timeout = timeout;
    dumpParameters.dumpType = DumpType::Hostboot;

    // will not return until dump is complete or timeout
    requestDump(dumpParameters);
}

void eventWatchdogTimeout(const uint32_t timeout)
{
    // Additional data to be added to PEL object
    // Currently we don't have anything to add
    // Keeping this for now in case if we have to add
    // any data corresponding to watchdog timeout
    std::map<std::string, std::string> additionalData;
    event(additionalData, timeout);
}

} // namespace dump
} // namespace watchdog

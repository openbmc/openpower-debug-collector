#include <watchdog_common.hpp>
#include <watchdog_logging.hpp>

namespace watchdog
{
namespace dump
{

void triggerHostbootDump(const uint32_t timeout)
{
    constexpr auto HOST_STATE_DIAGNOSTIC_MODE =
        "obmc-host-diagnostic-mode@0.target";
    constexpr auto HOST_STATE_QUIESCE_TGT = "obmc-host-quiesce@0.target";

    // Put system into diagnostic mode
    transitionHost(HOST_STATE_DIAGNOSTIC_MODE);

    // Collect Hostboot dump if auto reboot is enabled
    if (isAutoRebootEnabled())
    {
        eventWatchdogTimeout(timeout);
    }

    // Put system into quiesce state
    transitionHost(HOST_STATE_QUIESCE_TGT);
}

} // namespace dump
} // namespace watchdog

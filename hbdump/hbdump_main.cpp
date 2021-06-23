#include <hbdump_common.hpp>
#include <hbdump_logging.hpp>

namespace dump
{
namespace hbdump
{
void triggerHostbootDump()
{
    constexpr auto HOST_STATE_DIAGNOSTIC_MODE =
        "obmc-host-diagnostic-mode@0.target";
    constexpr auto HOST_STATE_QUIESCE_TGT = "obmc-host-quiesce@0.target";

    // Put system into diagnostic mode
    transitionHost(HOST_STATE_DIAGNOSTIC_MODE);

    // Collect Hostboot dump if auto reboot is enabled
    if (isAutoRebootEnabled())
    {
        eventWatchdogTimeout();
    }

    // Put system into quiesce state
    transitionHost(HOST_STATE_QUIESCE_TGT);
}

} // namespace hbdump
} // namespace dump

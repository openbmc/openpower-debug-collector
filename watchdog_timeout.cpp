#include "org/open_power/Host/Boot/error.hpp"
#include "phosphor-logging/elog-errors.hpp"

#include <phosphor-logging/elog.hpp>
#include <watchdog/watchdog_main.hpp>

int main(int /*argc*/, char** /*argv*/)
{
    using namespace phosphor::logging;
    using namespace watchdog::dump;

    triggerHostbootDump();

    using error =
        sdbusplus::org::open_power::Host::Boot::Error::WatchdogTimedOut;

    report<error>();

    return 0;
}

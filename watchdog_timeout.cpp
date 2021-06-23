#include "org/open_power/Host/Boot/error.hpp"
#include "phosphor-logging/elog-errors.hpp"

#include <hbdump/hbdump_main.hpp>
#include <phosphor-logging/elog.hpp>

int main(int /*argc*/, char** /*argv*/)
{
    using namespace phosphor::logging;
    // using namespace dump::hbdump;

    dump::hbdump::triggerHostbootDump();

    using error =
        sdbusplus::org::open_power::Host::Boot::Error::WatchdogTimedOut;

    report<error>();

    return 0;
}

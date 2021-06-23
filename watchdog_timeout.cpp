#include "org/open_power/Host/Boot/error.hpp"
#include "phosphor-logging/elog-errors.hpp"

#include <config.h>

#include <CLI/CLI.hpp>
#include <phosphor-logging/elog.hpp>
#include <watchdog/watchdog_main.hpp>

int main(int argc, char* argv[])
{
    CLI::App app{"Hostboot dump collection"};

    // Use default 1500 seconds timeout if dump progress status
    // remains in 'InProgress' state.
    constexpr uint32_t timeoutValue{1500};
    uint32_t timeoutInterval{timeoutValue};
    app.add_option("-t,--timeout", timeoutInterval,
                   "Set timeout interval for watchdog timeout in seconds");

    CLI11_PARSE(app, argc, argv);

    if constexpr (hbdumpCollectionEnabled)
    {
        using namespace watchdog::dump;
        triggerHostbootDump(timeoutInterval);
    }
    else
    {
        using namespace phosphor::logging;
        using error =
            sdbusplus::org::open_power::Host::Boot::Error::WatchdogTimedOut;
        report<error>();
    }

    return 0;
}

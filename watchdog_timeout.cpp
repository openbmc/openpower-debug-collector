#include <config.h>

#include <CLI/CLI.hpp>

#ifdef HOSTBOOT_DUMP_COLLECTION
#include <watchdog/watchdog_main.hpp>
#else
#include "org/open_power/Host/Boot/error.hpp"
#include "phosphor-logging/elog-errors.hpp"

#include <phosphor-logging/elog.hpp>
#endif

int main(int argc, char* argv[])
{
    CLI::App app{"Hostboot dump collector for watchdog timeout"};

#ifdef HOSTBOOT_DUMP_COLLECTION
    uint32_t timeoutInterval = 1500; // in seconds
    app.add_option("-t,--timeout", timeoutInterval,
                   "Set timeout interval for watchdog timeout in seconds");
#endif

    CLI11_PARSE(app, argc, argv);

#ifdef HOSTBOOT_DUMP_COLLECTION
    using namespace watchdog::dump;
    // TODO: trigger SBE dump if in SBE window otherwise hostboot dump
    triggerHostbootDump(timeoutInterval);
#else
    using namespace phosphor::logging;
    using error =
        sdbusplus::org::open_power::Host::Boot::Error::WatchdogTimedOut;
    report<error>();
#endif

    return 0;
}

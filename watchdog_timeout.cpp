#include <config.h>

#include <CLI/CLI.hpp>

#ifdef WATCHDOG_DUMP_COLLECTION
extern "C"
{
#include <libpdbg.h>
#include <libpdbg_sbe.h>
}

#include <fmt/format.h>
#include <libphal.H>

#include <phosphor-logging/log.hpp>
#include <watchdog/watchdog_common.hpp>
#include <watchdog/watchdog_dbus.hpp>
#include <watchdog/watchdog_main.hpp>
#else
#include "org/open_power/Host/Boot/error.hpp"
#include "phosphor-logging/elog-errors.hpp"

#include <phosphor-logging/elog.hpp>
#endif

int main(int argc, char* argv[])
{
    CLI::App app{"Hostboot dump collector for watchdog timeout"};

#ifdef WATCHDOG_DUMP_COLLECTION
    constexpr uint32_t dumpTimeout = 1500; // in seconds
    uint32_t timeout = dumpTimeout;
    app.add_option("-t,--timeout", timeout,
                   "Set timeout interval for watchdog timeout in seconds");
#endif

    CLI11_PARSE(app, argc, argv);

#ifdef WATCHDOG_DUMP_COLLECTION
    using namespace phosphor::logging;
    using namespace watchdog::dump;

    log<level::INFO>("Host did not respond within watchdog timeout interval");
    try
    {
        using namespace openpower::phal;

        // Initialize pdbg library, default parameters are used for init()
        pdbg::init();

        // Get Primary Proc
        struct pdbg_target* procTarget = pdbg::getPrimaryProc();

        // Check Primary IPL done
        bool primaryIplDone = sbe::isPrimaryIplDone();
        if (primaryIplDone)
        {
            // Collect hostboot dump only if the host is in 'Running' state
            if (!isHostStateRunning())
            {
                log<level::INFO>(
                    "CurrentHostState is not in 'Running' state. Dump maybe "
                    "already occurring, skipping this dump request...");
                return EXIT_SUCCESS;
            }

            // SBE boot done, Need to collect hostboot dump
            log<level::INFO>("Handle Hostboot boot failure");
            triggerHostbootDump(timeout);
        }
        else
        {
            // SBE boot window, handle SBE boot failure
            log<level::INFO>("Handle SBE boot failure");
            handleSbeBootError(procTarget, timeout);
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(fmt::format("Exception {} occurred", e.what()).c_str());
        std::string eventType =
            "org.open_power.Host.Boot.Error.WatchdogTimedOut";
        auto ffdc = std::vector<FFDCTuple>{};
        std::map<std::string, std::string> additionalData;

        if (!createPel(eventType, additionalData, ffdc))
        {
            log<level::ERR>("Failed to create PEL");
        }

        return EXIT_SUCCESS;
    }

#else
    using namespace phosphor::logging;
    using error =
        sdbusplus::org::open_power::Host::Boot::Error::WatchdogTimedOut;
    report<error>();
#endif

    return EXIT_SUCCESS;
}

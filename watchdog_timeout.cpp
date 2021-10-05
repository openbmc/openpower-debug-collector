#include <config.h>

#include <CLI/CLI.hpp>

#ifdef HOSTBOOT_DUMP_COLLECTION
extern "C"
{
#include <libpdbg.h>
}

#include <fmt/format.h>
#include <libpdbg_sbe.h>
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

#ifdef HOSTBOOT_DUMP_COLLECTION
    uint32_t timeoutInterval = 1500; // in seconds
    app.add_option("-t,--timeout", timeoutInterval,
                   "Set timeout interval for watchdog timeout in seconds");
#endif

    CLI11_PARSE(app, argc, argv);

#ifdef HOSTBOOT_DUMP_COLLECTION
    using namespace phosphor::logging;
    using namespace watchdog::dump;

    // TODO: trigger SBE dump if in SBE window otherwise hostboot dump
    log<level::INFO>("watchdog_timeout: SBE/Hostboot dump started!");

    try
    {
        pdbgInit();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(fmt::format("pdbgInit: Exception{}", e.what()).c_str());

        return -1;
    }

    struct pdbg_target* procTarget;
    pdbg_for_each_class_target("proc", procTarget)
    {
        if (isPrimaryProc(procTarget))
        {
            break;
        }
        procTarget = nullptr;
    }

    if (procTarget == nullptr)
    {
        log<level::ERR>("Failed to get target for primary processor");

        std::string eventType = "org.open_power.Processor.Error.SbeBootFailure";
        auto ffdc = std::vector<FFDCTuple>{};
        std::map<std::string, std::string> additionalData;
        additionalData.emplace("SRC6", "00000000"); // Is this correct?

        if (!createPel(eventType, additionalData, ffdc))
        {
            log<level::ERR>("Failed to create PEL");
        }
        return -1; // error number?
    }

    bool hbWindow = false;
    struct pdbg_target* pibTarget = nullptr;

    try
    {
        pibTarget = getPibTarget(procTarget);
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(
            fmt::format("getPibTarget: Exception{}", e.what()).c_str());

        return -1;
    }

    // Determine whether in SBE or Hostboot window
    if (!sbe_is_ipl_done(pibTarget, &hbWindow))
    {
        if (hbWindow)
        {
            log<level::INFO>("Hostboot window");
            triggerHostbootDump(timeoutInterval);
        }
        else
        {
            log<level::INFO>("SBE window");
            triggerCaptureFFDC(procTarget);
        }
    }
    else
    {
        log<level::ERR>("sbe_is_ipl_done: failed");
        return -1;
    }
#else
    using namespace phosphor::logging;
    using error =
        sdbusplus::org::open_power::Host::Boot::Error::WatchdogTimedOut;
    report<error>();
#endif

    return 0;
}

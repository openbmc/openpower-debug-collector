#include "config.h"

#include "dump_manager.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <fmt/core.h>

#include <phosphor-logging/elog-errors.hpp>
#include <sdbusplus/bus.hpp>

int main()
{
    using namespace phosphor::logging;
    using InternalFailure =
        sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

    auto bus = sdbusplus::bus::new_default();
    sd_event* event = nullptr;
    auto rc = sd_event_default(&event);
    if (rc < 0)
    {
        log<level::ERR>(
            fmt::format("Error occurred during the sd_event_default, rc({})",
                        rc)
                .c_str());
        report<InternalFailure>();
        return rc;
    }
    openpower::dump::EventPtr eventP{event};
    event = nullptr;

    // Blocking SIGCHLD is needed for calling sd_event_add_child
    sigset_t mask;
    if (sigemptyset(&mask) < 0)
    {
        log<level::ERR>(
            fmt::format("Unable to initialize signal set, errno({})", errno)
                .c_str());
        return EXIT_FAILURE;
    }

    if (sigaddset(&mask, SIGCHLD) < 0)
    {
        log<level::ERR>(
            fmt::format("Unable to add signal to signal set, errno({})", errno)
                .c_str());
        return EXIT_FAILURE;
    }

    // Block SIGCHLD first, so that the event loop can handle it
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) < 0)
    {
        log<level::ERR>(
            fmt::format("Unable to block signal, errno({})", errno).c_str());
        return EXIT_FAILURE;
    }

    // Add sdbusplus ObjectManager for the 'root' path of the DUMP manager.
    sdbusplus::server::manager::manager objManager(bus, OP_DUMP_OBJPATH);
    bus.request_name(OP_DUMP_BUSNAME);

    openpower::dump::Manager mgr(bus, OP_DUMP_OBJPATH, eventP);

    bus.attach_event(eventP.get(), SD_EVENT_PRIORITY_NORMAL);
    return sd_event_loop(eventP.get());
}

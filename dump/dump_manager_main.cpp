#include "config.h"

#include "dump_manager.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <sdbusplus/bus.hpp>
constexpr auto DUMP_OBJPATH = "/org/openpower/dump";
constexpr auto DUMP_BUSNAME = "org.open_power.Dump.Manager";
int main()
{
    using namespace phosphor::logging;
    auto bus = sdbusplus::bus::new_default();
    // Add sdbusplus ObjectManager for the 'root' path of the DUMP manager.
    sdbusplus::server::manager::manager objManager(bus, DUMP_OBJPATH);
    bus.request_name(DUMP_BUSNAME);

    auto event = sdeventplus::Event::get_default();

    openpower::dump::Manager mgr(bus, DUMP_OBJPATH, event);

    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
    event.loop();
    /*
            rc = sd_event_loop(eventP.get());
            if (rc < 0)
            {
                log<level::ERR>("Error occurred during the sd_event_loop",
                                entry("RC=%d", rc));
                throw std::runtime_error("Error occurred during the
       sd_event_loop1");
            }
    */
    return 0;
}

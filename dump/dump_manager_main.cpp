#include "config.h"

#include "dump_manager.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <sdbusplus/bus.hpp>
int main()
{
    using namespace phosphor::logging;
    auto bus = sdbusplus::bus::new_default();
    // Add sdbusplus ObjectManager for the 'root' path of the DUMP manager.
    sdbusplus::server::manager::manager objManager(bus, OP_DUMP_OBJPATH);
    bus.request_name(OP_DUMP_BUSNAME);

    auto event = sdeventplus::Event::get_default();

    openpower::dump::Manager mgr(bus, OP_DUMP_OBJPATH, event);

    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
    event.loop();
    return 0;
}

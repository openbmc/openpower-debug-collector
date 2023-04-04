#include "config.h"

#include "dump_manager.hpp"

#include <sdbusplus/bus.hpp>

int main()
{
    auto bus = sdbusplus::bus::new_default();

    // Add sdbusplus ObjectManager for the 'root' path of the DUMP manager.
    sdbusplus::server::manager::manager objManager(bus, OP_DUMP_OBJPATH);
    bus.request_name(OP_DUMP_BUSNAME);

    openpower::dump::Manager mgr(bus, OP_DUMP_OBJPATH);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    exit(EXIT_SUCCESS);
}

#include "config.h"

#include "dump_manager.hpp"

#include <sdbusplus/bus.hpp>

constexpr auto DUMP_OBJPATH = "/org/openpower/dump";
constexpr auto DUMP_BUSNAME = "org.open_power.Dump.Manager";
int main()
{
    auto bus = sdbusplus::bus::new_default();
    // Add sdbusplus ObjectManager for the 'root' path of the DUMP manager.
    sdbusplus::server::manager::manager objManager(bus, DUMP_OBJPATH);
    bus.request_name(DUMP_BUSNAME);

    openpower::dump::Manager mgr(bus, DUMP_OBJPATH);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}

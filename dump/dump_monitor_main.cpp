#include "dump_monitor.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>

#include <exception>
#include <iostream>
#include <variant>

int main()
{
    openpower::dump::DumpMonitor monitor;
    monitor.run();
    return 0;
}

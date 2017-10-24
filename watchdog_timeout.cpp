#include <phosphor-logging/elog.hpp>
#include "phosphor-logging/elog-errors.hpp"
#include "org/open_power/Host/Boot/error.hpp"

int main(int argc, char* argv[])
{
    using namespace phosphor::logging;
    using error = sdbusplus::org::open_power::Host::Boot::Error::WatchdogTimedOut;
    report<error>();

    return 0;
}


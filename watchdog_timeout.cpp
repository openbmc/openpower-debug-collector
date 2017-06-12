#include <phosphor-logging/elog.hpp>
#include "elog-errors.hpp"
#include "org/open_power/Host/error.hpp"

int main(int argc, char* argv[])
{
    using namespace phosphor::logging;
    using error = sdbusplus::org::open_power::Host::Error::WatchdogTimedOut;
    report<error>();

    return 0;
}


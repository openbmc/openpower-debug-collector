#include "org/open_power/Host/Boot/error.hpp"
#include "phosphor-logging/elog-errors.hpp"

#include <phosphor-logging/elog.hpp>

int main(int argc, char* argv[])
{
    using namespace phosphor::logging;
    using error = sdbusplus::org::open_power::Host::Boot::Error::Checkstop;
    report<error>();

    return 0;
}

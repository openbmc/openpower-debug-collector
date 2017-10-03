#include <phosphor-logging/elog.hpp>
#include "phosphor-logging/elog-errors.hpp"
#include "org/open_power/Host/error.hpp"
#include "org/open_power/Test/error.hpp"

int main(int argc, char* argv[])
{
    using namespace phosphor::logging;
    using error = sdbusplus::org::open_power::Host::Error::Checkstop;
    report<error>();

    using testerror = sdbusplus::org::open_power::Test::Error::TestFailure;
    report<testerror>();
    return 0;
}


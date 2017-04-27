#include <phosphor-logging/elog.hpp>
#include "elog-errors.hpp"

int main(int argc, char* argv[])
{
    using namespace phosphor::logging;
    using error = org::open_power::Host::Checkstop;
    report<error>();

    return 0;
}


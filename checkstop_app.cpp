#include <phosphor-logging/elog.hpp>
#include "elog-errors.hpp"

int main(int argc, char* argv[])
{
    using namespace phosphor::logging;
    using error = org::open_power::Common::BMC::Checkstop;
    report<error>(error::STRING("Checkstop"));

    return 0;
}


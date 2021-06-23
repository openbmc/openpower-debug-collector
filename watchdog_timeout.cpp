#include <watchdog/watchdog_main.hpp>

int main(int /*argc*/, char** /*argv*/)
{
    using namespace watchdog::dump;

    triggerHostbootDump();

    return 0;
}

#include <CLI/CLI.hpp>
#include <watchdog/watchdog_main.hpp>

int main(int argc, char* argv[])
{
    CLI::App app{"Hostboot dump collection"};

    // Use default 1500 seconds timeout if dump progress status
    // remains in 'InProgress' state.
    constexpr uint32_t timeoutValue{1500};
    uint32_t timeoutInterval{timeoutValue};
    app.add_option("-t,--timeout", timeoutInterval,
                   "Set timeout interval for watchdog timeout in seconds");

    CLI11_PARSE(app, argc, argv);

    using namespace watchdog::dump;

    triggerHostbootDump(timeoutInterval);

    return 0;
}

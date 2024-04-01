#include "sbe_consts.hpp"
#include "sbe_dump_collector.hpp"

#include <libphal.H>

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>

#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
    using namespace openpower::dump::sbe_chipop;
    using std::filesystem::path;
    using namespace openpower::dump::SBE;
    using namespace openpower::phal::dump;

    CLI::App app{"Dump Collector Application", "dump-collect"};
    app.description(
        "Collects dumps from the Self Boot Engine (SBE) based on "
        "provided parameters.\nSupports different types of dumps and requires "
        "specific options based on the dump type.");

    int type = 0;
    uint32_t id;
    std::string pathStr;
    std::optional<uint64_t> failingUnit;

    app.add_option("--type, -t", type, "Type of the dump")
        ->required()
        ->check(CLI::IsMember({SBE_DUMP_TYPE_HARDWARE, SBE_DUMP_TYPE_HOSTBOOT,
                               SBE_DUMP_TYPE_SBE, SBE_DUMP_TYPE_PERFORMANCE,
                               SBE_DUMP_TYPE_MSBE}));

    app.add_option("--id, -i", id, "ID of the dump")->required();

    app.add_option("--path, -p", pathStr,
                   "Path to store the collected dump files")
        ->required();

    app.add_option("--failingunit, -f", failingUnit, "ID of the failing unit");

    try
    {
        CLI11_PARSE(app, argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        return app.exit(e);
    }

    if (((type == SBE_DUMP_TYPE_HARDWARE) || (type == SBE_DUMP_TYPE_SBE) ||
         (type == SBE_DUMP_TYPE_MSBE)) &&
        !failingUnit.has_value())
    {
        std::cerr
            << "Failing unit ID is required for Hardware and SBE type dumps\n";
        return EXIT_FAILURE;
    }

    // Directory creation should happen here, after successful parsing
    std::filesystem::path dirPath{pathStr};
    if (!std::filesystem::exists(dirPath))
    {
        std::filesystem::create_directories(dirPath);
    }

    SbeDumpCollector dumpCollector;

    auto failingUnitId = 0xFFFFFF; // Default or unspecified value
    if (failingUnit.has_value())
    {
        failingUnitId = failingUnit.value();
    }

    try
    {
        if ((type == SBE_DUMP_TYPE_SBE) || (type == SBE_DUMP_TYPE_MSBE))
        {
            collectSBEDump(id, failingUnitId, pathStr, type);
        }
        else
        {
            dumpCollector.collectDump(type, id, failingUnitId, pathStr);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to collect dump: " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }

    return 0;
}

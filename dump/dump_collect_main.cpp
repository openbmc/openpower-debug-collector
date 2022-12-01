#include "argument.hpp"
#include "dump_collect.hpp"
#include "sbe_consts.hpp"

#include <libphal.H>

#include <filesystem>
#include <iostream>

namespace openpower
{
namespace dump
{
namespace util
{

static void ExitWithError(const std::string& err,
                          const std::string& programName)
{
    ArgumentParser::usage(programName);
    std::cerr << std::endl << "ERROR: " << err << std::endl;
    std::exit(EXIT_FAILURE);
}

} // namespace util
} // namespace dump
} // namespace openpower

int main(int argc, char** argv)
{
    using namespace openpower::dump::util;
    using namespace openpower::dump::sbe_chipop;
    using std::filesystem::path;
    using namespace openpower::dump::SBE;
    using namespace openpower::phal::dump;

    // Instantiate the argument parser
    ArgumentParser options(argc, argv);

    // Parse arguments
    auto typeOpt = options.getInt("type");
    if (!typeOpt.has_value())
    {
        ExitWithError("Type not specified", argv[0]);
    }

    auto type = typeOpt.value();
    if (!((type == SBE_DUMP_TYPE_HOSTBOOT) ||
          (type == SBE_DUMP_TYPE_HARDWARE) ||
          (type == SBE_DUMP_TYPE_PERFORMANCE) || (type == SBE_DUMP_TYPE_SBE)))
    {
        ExitWithError("Type specified is invalid.", argv[0]);
    }

    auto idOpt = options.getInt("id");
    if (!idOpt.has_value())
    {
        ExitWithError("Dump id is not provided", argv[0]);
    }
    auto id = idOpt.value();

    auto pathOpt = options.getString("path");
    if (!pathOpt.has_value())
    {
        ExitWithError("Collection path not specified.", argv[0]);
    }
    auto path = pathOpt.value();

    auto failingUnitOpt = options.getInt("failingunit");
    auto failingUnit = 0xFFFFFF; // Default or unspecified value
    if (failingUnitOpt.has_value())
    {
        failingUnit = failingUnitOpt.value();
    }

    SbeDumpCollector dumpCollector;

    try
    {
        if (type == SBE_DUMP_TYPE_SBE)
        {
            collectSBEDump(id, failingUnit, path, type);
        }
        else
        {
            dumpCollector.collectDump(type, id, failingUnit, path);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to collect dump: " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }

    return 0;
}

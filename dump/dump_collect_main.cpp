#include "argument.hpp"
#include "dump_collect.hpp"
#include "sbe_consts.hpp"

#include <libphal.H>

#include <iostream>
#include <string>

static void ExitWithError(const char* err, char** argv)
{
    openpower::dump::util::ArgumentParser::usage(argv);
    std::cerr << std::endl;
    std::cerr << "ERROR: " << err << std::endl;
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    // Read arguments.
    auto options = openpower::dump::util::ArgumentParser(argc, argv);

    // Parse arguments
    std::string typeStr = std::move((options)["type"]);
    if (typeStr.empty())
    {
        ExitWithError("type not specified", argv);
    }

    auto type = std::stoi(typeStr);
    if (!((type == openpower::dump::SBE::SBE_DUMP_TYPE_HOSTBOOT) ||
          (type == openpower::dump::SBE::SBE_DUMP_TYPE_HARDWARE) ||
          (type == openpower::dump::SBE::SBE_DUMP_TYPE_SBE)))
    {
        ExitWithError("type specified is invalid.", argv);
    }

    std::string idStr = std::move((options)["id"]);
    if (idStr.empty())
    {
        ExitWithError("Dump id is not provided", argv);
    }
    auto id = std::stoi(idStr);

    std::filesystem::path path = std::move((options)["path"]);
    if (path.empty())
    {
        ExitWithError("Collection path not specified.", argv);
    }

    std::string failingUnitStr = std::move((options)["failingunit"]);
    auto failingUnit = std::stoi(failingUnitStr);
    try
    {
        if (type == openpower::dump::SBE::SBE_DUMP_TYPE_SBE)
        {
            openpower::phal::dump::collectSBEDump(id, failingUnit, path);
        }
        else
        {
            openpower::dump::sbe_chipop::collectDump(type, id, failingUnit,
                                                     path);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
}

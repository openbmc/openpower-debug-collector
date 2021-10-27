#include "argument.hpp"

#include <getopt.h>

#include <iostream>

namespace openpower
{
namespace dump
{
namespace util
{

void ArgumentParser::parseArguments(int argc, char** argv)
{
    option longOptions[] = {{"help", no_argument, nullptr, 'h'},
                            {"type", required_argument, nullptr, 't'},
                            {"id", required_argument, nullptr, 'i'},
                            {"path", required_argument, nullptr, 'p'},
                            {"failingunit", required_argument, nullptr, 'f'},
                            {nullptr, 0, nullptr, 0}};
    const char* shortOptions = "ht:i:p:f:";
    int optionIndex = 0;
    int c;

    while ((c = getopt_long(argc, argv, shortOptions, longOptions,
                            &optionIndex)) != -1)
    {
        switch (c)
        {
            case 'h':
                usage(argv[0]); // Here, we use argv[0] to print the program's
                                // name.
                exit(EXIT_SUCCESS);
            case 't':
            case 'i':
            case 'p':
            case 'f':
                arguments[longOptions[optionIndex].name] = std::string(optarg);
                break;
            default:
                throw std::invalid_argument("Unknown option");
        }
    }
}

std::optional<std::string>
    ArgumentParser::getString(const std::string& option) const
{
    if (auto it = arguments.find(option); it != arguments.end())
    {
        return std::get<std::string>(it->second);
    }
    return std::nullopt;
}

std::optional<int> ArgumentParser::getInt(const std::string& option) const
{
    if (auto strVal = getString(option))
    {
        return std::stoi(*strVal);
    }
    return std::nullopt;
}

bool ArgumentParser::getBool(const std::string& option) const
{
    return arguments.find(option) != arguments.end();
}

void ArgumentParser::usage(const std::string& programName)
{
    std::cerr << "Usage: " << programName << " [options]\n";
    std::cerr << "Options:\n";
    std::cerr << "    --help            Print this menu\n";
    std::cerr << "    --type            Type of the dump\n";
    std::cerr
        << "                      Valid types: 0 - Hardware, 5 - Hostboot\n";
    std::cerr << "    --id              ID of the dump\n";
    std::cerr << "    --path            Path to store dump\n";
    std::cerr << "    --failingunit     ID of the failing unit\n";
    std::cerr << std::flush;
}

ArgumentParser::ArgumentParser(int argc, char** argv)
{
    parseArguments(argc, argv);
}

} // namespace util
} // namespace dump
} // namespace openpower

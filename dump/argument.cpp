#include "argument.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>

namespace openpower
{
namespace dump
{
namespace util
{

ArgumentParser::ArgumentParser(int argc, char** argv)
{
    auto option = 0;
    while (-1 !=
           (option = getopt_long(argc, argv, optionstr, options, nullptr)))
    {
        if ((option == '?') || (option == 'h'))
        {
            usage(argv);
            exit(-1);
        }

        auto i = &options[0];
        while ((i->val != option) && (i->val != 0))
        {
            ++i;
        }

        if (i->val)
        {
            arguments[i->name] = (i->has_arg ? optarg : true_string);
        }
    }
}

const std::string& ArgumentParser::operator[](const std::string& opt)
{
    auto elem = arguments.find(opt);
    if (elem == arguments.end())
    {
        return empty_string;
    }
    else
    {
        return elem->second;
    }
}

void ArgumentParser::usage(char** argv)
{
    std::cerr << "Usage: " << argv[0] << " [options]\n";
    std::cerr << "Options:\n";
    std::cerr << "    --help            Print this menu\n";
    std::cerr << "    --type            type of dump\n";
    std::cerr
        << "                      Valid types:0 - Hardware, 5 - Hostboot \n";
    std::cerr << "    --id              Id of the dump\n";
    std::cerr << "    --path            path to store dump\n";
    std::cerr << "    --failingunit     Id of the failing unit\n";
    std::cerr << std::flush;
}

const option ArgumentParser::options[] = {
    {"type", required_argument, nullptr, 't'},
    {"id", required_argument, nullptr, 'i'},
    {"path", required_argument, nullptr, 'p'},
    {"failingunit", required_argument, nullptr, 'f'},
    {"help", no_argument, nullptr, 'h'},
    {0, 0, 0, 0},
};

const char* ArgumentParser::optionstr = "tipfh?";

const std::string ArgumentParser::true_string = "true";
const std::string ArgumentParser::empty_string = "";

} // namespace util
} // namespace dump
} // namespace openpower

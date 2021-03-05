#include "dump_utils.hpp"

#include <phosphor-logging/log.hpp>
#include <string>

// Mapper
constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";

using namespace phosphor::logging;

std::string getService(sdbusplus::bus::bus& bus, const std::string& intf,
                       const std::string& path)
{

    auto mapper = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetObject");

    mapper.append(path);
    mapper.append(std::vector<std::string>({intf}));

    auto mapperResponseMsg = bus.call(mapper);

    if (mapperResponseMsg.is_method_error())
    {
        // TODO openbmc/openbmc#851 - Once available, throw returned error
        log<level::ERR>("ERROR in mapper call");
        throw std::runtime_error("ERROR in mapper call");
    }

    std::map<std::string, std::vector<std::string>> mapperResponse;
    mapperResponseMsg.read(mapperResponse);

    if (mapperResponse.empty())
    {
        // TODO openbmc/openbmc#1712 - Handle empty mapper resp. consistently
        log<level::ERR>("ERROR in reading the mapper response");
        throw std::runtime_error("ERROR in reading the mapper response");
    }

    return mapperResponse.begin()->first;
}

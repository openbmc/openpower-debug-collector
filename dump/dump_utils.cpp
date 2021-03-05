#include "dump_utils.hpp"

#include <phosphor-logging/log.hpp>

#include <string>

namespace openpower
{
namespace dump
{
namespace util
{
// Mapper
constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";

using namespace phosphor::logging;

std::string getService(sdbusplus::bus::bus& bus, const std::string& intf,
                       const std::string& path)
{
    try
    {
        auto mapper = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetObject");

        mapper.append(path, std::vector<std::string>({intf}));

        auto mapperResponseMsg = bus.call(mapper);
        std::map<std::string, std::vector<std::string>> mapperResponse;
        mapperResponseMsg.read(mapperResponse);

        if (mapperResponse.empty())
        {
            log<level::ERR>("ERROR in reading the mapper response");
            throw std::runtime_error("ERROR in reading the mapper response");
        }
        return mapperResponse.begin()->first;
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("Mapper call failed", entry("METHOD=%d", "GetObject"),
                        entry("ERROR=%s", ex.what()),
                        entry("PATH=%s", path.c_str()),
                        entry("INTERFACE=%s", intf.c_str()));
        throw std::runtime_error("Mapper call failed");
    }
}
} // namespace util
} // namespace dump
} // namespace openpower

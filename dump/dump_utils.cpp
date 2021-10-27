#include "dump_utils.hpp"

#include <fmt/core.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>

#include <fstream>
#include <string>

namespace openpower
{
namespace dump
{
namespace util
{
using namespace phosphor::logging;

std::string getService(sdbusplus::bus::bus& bus, const std::string& intf,
                       const std::string& path)
{
    constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
    constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
    constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
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
            log<level::ERR>(fmt::format("Empty mapper response for GetObject "
                                        "interface({}), path({})",
                                        intf, path)
                                .c_str());
            throw std::runtime_error("Empty mapper response for GetObject");
        }
        return mapperResponse.begin()->first;
    }
    catch (const sdbusplus::exception::exception& ex)
    {
        log<level::ERR>(fmt::format("Mapper call failed for GetObject "
                                    "errorMsg({}), path({}), interface({}) ",
                                    ex.what(), path, intf)
                            .c_str());
        throw;
    }
}

void prepareCollection(const std::filesystem::path& dumpPath,
                       const std::string& errorLogId)
{
    using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
    try
    {
        std::filesystem::create_directories(dumpPath);
        auto elogPath = dumpPath.parent_path();
        elogPath /= "ErrorLog";
        std::ofstream outfile{elogPath, std::ios::out | std::ios::binary};
        if (!outfile.good())
        {
            using metadata = xyz::openbmc_project::Common::File::Open;
            // Unable to open the file for writing
            auto err = errno;
            log<level::ERR>(
                fmt::format("Error opening file to write errorlog id, "
                            "errno({}), filepath({})",
                            err, dumpPath.string())
                    .c_str());
            // Report the error and continue collection even if the error log id
            // cannot be added
            report<Open>(metadata::ERRNO(err),
                         metadata::PATH(dumpPath.c_str()));
        }
        else
        {
            outfile.exceptions(std::ifstream::failbit | std::ifstream::badbit |
                               std::ifstream::eofbit);
            outfile << errorLogId;
            outfile.close();
        }
    }
    catch (std::filesystem::filesystem_error& e)
    {
        log<level::ERR>(fmt::format("Error creating dump directories, path({})",
                                    dumpPath.string())
                            .c_str());
        throw;
    }
    catch (std::ofstream::failure& oe)
    {
        using metadata = xyz::openbmc_project::Common::File::Write;
        // If there is an error commit the error and continue.
        log<level::ERR>(fmt::format("Failed to write errorlog id to file, "
                                    "errorMsg({}), error({}), filepath({})",
                                    oe.what(), oe.code().value(),
                                    dumpPath.string())
                            .c_str());
        // Report the error and continue with dump collection
        // even if the error log id cannot be written to the file.
        report<Write>(metadata::ERRNO(oe.code().value()),
                      metadata::PATH(dumpPath.c_str()));
    }
}

} // namespace util
} // namespace dump
} // namespace openpower

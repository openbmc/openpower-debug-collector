#include "create_pel.hpp"

#include "dump_utils.hpp"

#include <fcntl.h>
#include <libekb.H>
#include <unistd.h>

#include <phosphor-logging/elog.hpp>
#include <xyz/openbmc_project/Logging/Create/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace openpower
{
using namespace phosphor::logging;

namespace dump
{
namespace pel
{

constexpr auto loggingObjectPath = "/xyz/openbmc_project/logging";
constexpr auto loggingInterface = "xyz.openbmc_project.Logging.Create";
constexpr auto opLoggingInterface = "org.open_power.Logging.PEL";

uint32_t createSbeErrorPEL(const std::string& event, const sbeError_t& sbeError,
                           const FFDCData& ffdcData, const Severity& severity)
{
    uint32_t plid = 0;
    std::unordered_map<std::string, std::string> additionalData = {
        {"_PID", std::to_string(getpid())}, {"SBE_ERR_MSG", sbeError.what()}};
    auto bus = sdbusplus::bus::new_default();

    additionalData.emplace("_PID", std::to_string(getpid()));
    additionalData.emplace("SBE_ERR_MSG", sbeError.what());

    for (auto& data : ffdcData)
    {
        additionalData.emplace(data);
    }

    std::vector<std::tuple<
        sdbusplus::xyz::openbmc_project::Logging::server::Create::FFDCFormat,
        uint8_t, uint8_t, sdbusplus::message::unix_fd>>
        pelFFDCInfo;

    // get SBE ffdc file descriptor
    auto fd = sbeError.getFd();

    // Negative fd value indicates error case or invalid file
    // No need of special processing , just log error with additional ffdc.
    if (fd > 0)
    {
        // Refer phosphor-logging/extensions/openpower-pels/README.md section
        // "Self Boot Engine(SBE) First Failure Data Capture(FFDC) Support"
        // for details of related to createPEL with SBE FFDC information
        // usin g CreateWithFFDCFiles api.
        pelFFDCInfo.emplace_back(
            std::make_tuple(sdbusplus::xyz::openbmc_project::Logging::server::
                                Create::FFDCFormat::Custom,
                            static_cast<uint8_t>(0xCB),
                            static_cast<uint8_t>(0x01), sbeError.getFd()));
    }
    try
    {
        auto service = util::getService(bus, opLoggingInterface,
                                        loggingObjectPath);
        auto method = bus.new_method_call(service.c_str(), loggingObjectPath,
                                          opLoggingInterface,
                                          "CreatePELWithFFDCFiles");
        auto level =
            sdbusplus::xyz::openbmc_project::Logging::server::convertForMessage(
                severity);
        method.append(event, level, additionalData, pelFFDCInfo);
        auto response = bus.call(method);

        // reply will be tuple containing bmc log id, platform log id
        std::tuple<uint32_t, uint32_t> reply = {0, 0};

        // parse dbus response into reply
        response.read(reply);
        plid = std::get<1>(reply); // platform log id is tuple "second"
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>(std::format("D-Bus call exception",
                                    "OBJPATH={}, INTERFACE={}, EXCEPTION={}",
                                    loggingObjectPath, loggingInterface,
                                    e.what())
                            .c_str());
        throw;
    }
    catch (const std::exception& e)
    {
        throw;
    }

    return plid;
}

FFDCFile::FFDCFile(const json& pHALCalloutData) :
    calloutData(pHALCalloutData.dump()),
    calloutFile("/tmp/phalPELCalloutsJson.XXXXXX"), fileFD(-1)
{
    prepareFFDCFile();
}

FFDCFile::~FFDCFile()
{
    removeCalloutFile();
}

int FFDCFile::getFileFD() const
{
    return fileFD;
}

void FFDCFile::prepareFFDCFile()
{
    createCalloutFile();
    writeCalloutData();
    setCalloutFileSeekPos();
}

void FFDCFile::createCalloutFile()
{
    fileFD = mkostemp(const_cast<char*>(calloutFile.c_str()), O_RDWR);

    if (fileFD == -1)
    {
        log<level::ERR>(std::format("Failed to create phalPELCallouts "
                                    "file({}), errorno({}) and errormsg({})",
                                    calloutFile, errno, strerror(errno))
                            .c_str());
        throw std::runtime_error("Failed to create phalPELCallouts file");
    }
}

void FFDCFile::writeCalloutData()
{
    ssize_t rc = write(fileFD, calloutData.c_str(), calloutData.size());

    if (rc == -1)
    {
        log<level::ERR>(std::format("Failed to write phaPELCallout info "
                                    "in file({}), errorno({}), errormsg({})",
                                    calloutFile, errno, strerror(errno))
                            .c_str());
        throw std::runtime_error("Failed to write phalPELCallouts info");
    }
    else if (rc != static_cast<ssize_t>(calloutData.size()))
    {
        log<level::WARNING>(std::format("Could not write all phal callout "
                                        "info in file({}), written byte({}) "
                                        "and total byte({})",
                                        calloutFile, rc, calloutData.size())
                                .c_str());
    }
}

void FFDCFile::setCalloutFileSeekPos()
{
    int rc = lseek(fileFD, 0, SEEK_SET);

    if (rc == -1)
    {
        log<level::ERR>(std::format("Failed to set SEEK_SET for "
                                    "phalPELCallouts in file({}), errorno({}) "
                                    "and errormsg({})",
                                    calloutFile, errno, strerror(errno))
                            .c_str());
        throw std::runtime_error(
            "Failed to set SEEK_SET for phalPELCallouts file");
    }
}

void FFDCFile::removeCalloutFile()
{
    close(fileFD);
    std::remove(calloutFile.c_str());
}

} // namespace pel
} // namespace dump
} // namespace openpower

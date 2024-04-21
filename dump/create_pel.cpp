#include "create_pel.hpp"

#include "dump_utils.hpp"
#include "sbe_consts.hpp"

#include <fcntl.h>
#include <libekb.H>
#include <unistd.h>

#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
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

namespace openpower::dump::pel
{

using namespace phosphor::logging;
using namespace openpower::dump::SBE;
constexpr auto loggingObjectPath = "/xyz/openbmc_project/logging";
constexpr auto loggingInterface = "xyz.openbmc_project.Logging.Create";
constexpr auto opLoggingInterface = "org.open_power.Logging.PEL";

uint32_t createSbeErrorPEL(const std::string& event, const sbeError_t& sbeError,
                           const FFDCData& ffdcData, const Severity severity,
                           const std::optional<PELFFDCInfo>& pelFFDCInfoOpt)
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

    PELFFDCInfo pelFFDCInfo;
    if (pelFFDCInfoOpt)
    {
        pelFFDCInfo = *pelFFDCInfoOpt;
    }

    // Negative fd value indicates error case or invalid file
    // No need of special processing , just log error with additional ffdc.
    else if (sbeError.getFd() > 0)
    {
        // Refer phosphor-logging/extensions/openpower-pels/README.md section
        // "Self Boot Engine(SBE) First Failure Data Capture(FFDC) Support"
        // for details of related to createPEL with SBE FFDC information
        // usin g CreateWithFFDCFiles api.
        pelFFDCInfo.emplace_back(std::make_tuple(
            sdbusplus::xyz::openbmc_project::Logging::server::Create::
                FFDCFormat::Custom,
            FFDC_FORMAT_SUBTYPE, FFDC_FORMAT_VERSION, sbeError.getFd()));
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
        lg2::error(
            "D-Bus call exception OBJPATH={OBJPATH}, INTERFACE={INTERFACE}, "
            "EXCEPTION={ERROR}",
            "OBJPATH", loggingObjectPath, "INTERFACE", loggingInterface,
            "ERROR", e);

        throw;
    }
    catch (const std::exception& e)
    {
        throw;
    }

    return plid;
}

openpower::dump::pel::Severity convertSeverityToEnum(uint8_t severity)
{
    switch (severity)
    {
        case openpower::phal::FAPI2_ERRL_SEV_RECOVERED:
            return openpower::dump::pel::Severity::Informational;
        case openpower::phal::FAPI2_ERRL_SEV_PREDICTIVE:
            return openpower::dump::pel::Severity::Warning;
        case openpower::phal::FAPI2_ERRL_SEV_UNRECOVERABLE:
            return openpower::dump::pel::Severity::Error;
        default:
            return openpower::dump::pel::Severity::Error;
    }
}

void processFFDCPackets(const openpower::phal::sbeError_t& sbeError,
                        const std::string& event,
                        openpower::dump::pel::FFDCData& pelAdditionalData)
{
    const auto& ffdcFileList = sbeError.getFfdcFileList();
    for (const auto& [slid, ffdcTuple] : ffdcFileList)
    {
        uint8_t severity;
        int fd;
        std::filesystem::path path;
        std::tie(severity, fd, path) = ffdcTuple;

        Severity logSeverity = convertSeverityToEnum(severity);

        PELFFDCInfo pelFFDCInfo;
        pelFFDCInfo.emplace_back(
            std::make_tuple(sdbusplus::xyz::openbmc_project::Logging::server::
                                Create::FFDCFormat::Custom,
                            FFDC_FORMAT_SUBTYPE, FFDC_FORMAT_VERSION, fd));

        auto logId = openpower::dump::pel::createSbeErrorPEL(
            event, sbeError, pelAdditionalData, logSeverity);
        lg2::info("Logged PEL {PELID} for SLID {SLID}", "PELID", logId, "SLID",
                  slid);
    }
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
        lg2::error("Failed to create phalPELCallouts file({FILE}), "
                   "errorno({ERRNO}) and errormsg({ERRORMSG})",
                   "FILE", calloutFile, "ERRNO", errno, "ERRORMSG",
                   strerror(errno));
        throw std::runtime_error("Failed to create phalPELCallouts file");
    }
}

void FFDCFile::writeCalloutData()
{
    ssize_t rc = write(fileFD, calloutData.c_str(), calloutData.size());

    if (rc == -1)
    {
        lg2::error("Failed to write phaPELCallout info in file({FILE}), "
                   "errorno({ERRNO}), errormsg({ERRORMSG})",
                   "FILE", calloutFile, "ERRNO", errno, "ERRORMSG",
                   strerror(errno));
        throw std::runtime_error("Failed to write phalPELCallouts info");
    }
    else if (rc != static_cast<ssize_t>(calloutData.size()))
    {
        lg2::warning("Could not write all phal callout info in file({FILE}), "
                     "written byte({WRITTEN}), total byte({TOTAL})",
                     "FILE", calloutFile, "WRITTEN", rc, "TOTAL",
                     calloutData.size());
    }
}

void FFDCFile::setCalloutFileSeekPos()
{
    int rc = lseek(fileFD, 0, SEEK_SET);

    if (rc == -1)
    {
        lg2::error("Failed to set SEEK_SET for phalPELCallouts in "
                   "file({FILE}), errorno({ERRNO}), errormsg({ERRORMSG})",
                   "FILE", calloutFile, "ERRNO", errno, "ERRORMSG",
                   strerror(errno));

        throw std::runtime_error(
            "Failed to set SEEK_SET for phalPELCallouts file");
    }
}

void FFDCFile::removeCalloutFile()
{
    close(fileFD);
    std::remove(calloutFile.c_str());
}

} // namespace openpower::dump::pel

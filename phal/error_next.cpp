#ifdef USE_PHAL_NEXT

#include "error_iface.hpp"
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Logging/Create/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <map>
#include <string>
#include <tuple>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace openpower::dump::phal::error {

namespace {

constexpr auto loggingObjectPath  = "/xyz/openbmc_project/logging";
constexpr auto opLoggingInterface = "org.open_power.Logging.PEL";

const char* errorTypeStr(chipop::ChipOpError::Type t) noexcept
{
    switch (t)
    {
        case chipop::ChipOpError::Type::NotAllowed:   return "NotAllowed";
        case chipop::ChipOpError::Type::Timeout:      return "Timeout";
        case chipop::ChipOpError::Type::Failed:       return "Failed";
        case chipop::ChipOpError::Type::NoFfdc:       return "NoFfdc";
        case chipop::ChipOpError::Type::InternalFfdc: return "InternalFfdc";
        default:                                       return "Unknown";
    }
}

} // anonymous namespace

bool logChipOpError(const chipop::ChipOpError& err,
                    targeting::TargetHandle chip,
                    [[maybe_unused]] uint32_t cmdClass,
                    [[maybe_unused]] uint32_t cmdType,
                    [[maybe_unused]] const std::filesystem::path& dumpPath)
{
    uint32_t chipPos = targeting::chipPos(chip);
    std::string chipPath = targeting::debugPath(chip);
    auto chipKind = targeting::kind(chip);
    std::string chipType = (chipKind == targeting::NodeKind::OcmbChip) ? "OCMB" : "PROC";

    // Determine if error is critical (exclude target) or recoverable
    bool isCritical = (err.type == chipop::ChipOpError::Type::NotAllowed ||
                       err.type == chipop::ChipOpError::Type::Timeout);

    lg2::error("Chip-op error on {TYPE} chip at position {POS} (path: {PATH}): "
               "{MSG} (errType={ERRTYPE}, critical={CRITICAL})",
               "TYPE", chipType, "POS", chipPos, "PATH", chipPath,
               "MSG", err.msg, "ERRTYPE", errorTypeStr(err.type),
               "CRITICAL", isCritical);

    return !isCritical;  // true = continue, false = exclude target
}

uint32_t createChipOpErrorPEL(const chipop::ChipOpError& err,
                               targeting::TargetHandle chip,
                               const std::string& event,
                               [[maybe_unused]] const std::filesystem::path& dumpPath)
{
    uint32_t logId = 0;
    uint32_t chipPos = targeting::chipPos(chip);
    auto chipKind = targeting::kind(chip);
    std::string chipType = (chipKind == targeting::NodeKind::OcmbChip) ? "OCMB" : "PROC";

    std::unordered_map<std::string, std::string> additionalData = {
        {"_PID",          std::to_string(getpid())},
        {"CHIP_OP_ERROR", err.msg},
        {"CHIP_TYPE",     chipType},
        {"CHIP_POS",      std::to_string(chipPos)},
        {"ERROR_TYPE",    errorTypeStr(err.type)},
    };

    // Severity: NotAllowed / InternalFfdc are informational; others are errors
    using Severity = sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;
    Severity severity = Severity::Error;
    if (err.type == chipop::ChipOpError::Type::NotAllowed ||
        err.type == chipop::ChipOpError::Type::InternalFfdc)
    {
        severity = Severity::Informational;
    }

    try
    {
        auto bus = sdbusplus::bus::new_default();

        // Locate the PEL service via object mapper
        auto mapperMethod = bus.new_method_call(
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper",
            "GetObject");
        mapperMethod.append(loggingObjectPath,
                            std::vector<std::string>{opLoggingInterface});
        auto mapperReply = bus.call(mapperMethod);
        std::map<std::string, std::vector<std::string>> mapperResp;
        mapperReply.read(mapperResp);
        if (mapperResp.empty())
        {
            lg2::error("PEL service not found for interface {INTF}",
                       "INTF", opLoggingInterface);
            return 0;
        }
        const std::string& service = mapperResp.begin()->first;

        // Call CreatePELWithFFDCFiles (no FFDC file — ChipOpError has no fd)
        using FFDCFormat =
            sdbusplus::xyz::openbmc_project::Logging::server::Create::FFDCFormat;
        using PELFFDCInfo =
            std::vector<std::tuple<FFDCFormat, uint8_t, uint8_t,
                                   sdbusplus::message::unix_fd>>;

        auto method = bus.new_method_call(
            service.c_str(), loggingObjectPath,
            opLoggingInterface, "CreatePELWithFFDCFiles");

        auto levelStr =
            sdbusplus::xyz::openbmc_project::Logging::server::convertForMessage(
                severity);
        PELFFDCInfo emptyFfdc;
        method.append(event, levelStr, additionalData, emptyFfdc);
        auto response = bus.call(method);

        // Reply is (uint32_t bmcLogId, uint32_t platformLogId)
        std::tuple<uint32_t, uint32_t> reply{0, 0};
        response.read(reply);
        logId = std::get<0>(reply);

        lg2::info("Created PEL logId={LOGID} for chip-op error on "
                  "{TYPE} pos {POS}",
                  "LOGID", logId, "TYPE", chipType, "POS", chipPos);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("D-Bus exception creating chip-op PEL: {ERROR}", "ERROR", e);
    }
    catch (const std::exception& e)
    {
        lg2::error("Exception creating chip-op PEL: {ERROR}", "ERROR", e.what());
    }

    return logId;
}

} // namespace openpower::dump::phal::error

#endif // USE_PHAL_NEXT

// Made with Bob

#include "error_iface.hpp"

#include "dump_utils.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <errl_handle.H>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/message.hpp>
#include <xyz/openbmc_project/Logging/Create/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace openpower::dump::phal::error
{
namespace
{

constexpr auto loggingObjectPath = "/xyz/openbmc_project/logging";
constexpr auto opLoggingInterface = "org.open_power.Logging.PEL";

using DBusFormat =
    sdbusplus::xyz::openbmc_project::Logging::server::Create::FFDCFormat;
using DBusLevel =
    sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;
using FFDCFiles =
    std::vector<std::tuple<DBusFormat, uint8_t, uint8_t,
                           sdbusplus::message::unix_fd>>;

DBusFormat convertFormat(errl::FFDCFormat format) noexcept
{
    switch (format)
    {
        case errl::FFDCFormat::JSON:
            return DBusFormat::JSON;
        case errl::FFDCFormat::CBOR:
            return DBusFormat::CBOR;
        case errl::FFDCFormat::Text:
            return DBusFormat::Text;
        case errl::FFDCFormat::Custom:
            return DBusFormat::Custom;
    }
    return DBusFormat::Custom;
}

DBusLevel convertSeverity(errl::Level level) noexcept
{
    switch (level)
    {
        case errl::Level::Emergency:
        case errl::Level::Alert:
        case errl::Level::Critical:
            return DBusLevel::Critical;
        case errl::Level::Error:
            return DBusLevel::Error;
        case errl::Level::Warning:
            return DBusLevel::Warning;
        case errl::Level::Notice:
        case errl::Level::Informational:
        case errl::Level::Debug:
            return DBusLevel::Informational;
    }
    return DBusLevel::Error;
}

FFDCFiles buildFFDCFiles(const errl::ErrlEntry& entry,
                         std::vector<int>& openedFds)
{
    FFDCFiles files;
    const auto& ffdc = entry.getFfdcFiles();
    if (!ffdc)
    {
        return files;
    }

    for (const auto& [file, path] : *ffdc)
    {
        int fd = file.fd;
        if (fd < 0)
        {
            if (path.empty())
            {
                lg2::warning("HostFW FFDC has no file descriptor or path");
                continue;
            }

            fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
            if (fd < 0)
            {
                lg2::warning("Cannot open HostFW FFDC file {PATH}", "PATH",
                             path.string());
                continue;
            }
            openedFds.push_back(fd);
        }

        files.emplace_back(convertFormat(file.format), file.subType,
                           file.version, sdbusplus::message::unix_fd{fd});
    }
    return files;
}

uint32_t commitEntry(sdbusplus::bus_t& bus, const errl::ErrlEntry& entry)
{
    std::map<std::string, std::string> additionalData = {
        {"_PID", std::to_string(::getpid())}};
    for (const auto& [key, value] : entry.getAdditionalData())
    {
        additionalData.insert_or_assign(key, value);
    }

    std::vector<int> openedFds;
    auto files = buildFFDCFiles(entry, openedFds);

    uint32_t logId = 0;
    try
    {
        const auto service = util::getService(
            bus, opLoggingInterface, loggingObjectPath);
        auto method = bus.new_method_call(
            service.c_str(), loggingObjectPath, opLoggingInterface,
            "CreatePELWithFFDCFiles");
        const auto level =
            sdbusplus::xyz::openbmc_project::Logging::server::
                convertForMessage(convertSeverity(entry.getSeverity()));
        method.append(entry.getMessage(), level, additionalData, files);

        auto reply = bus.call(method);
        std::tuple<uint32_t, uint32_t> ids{0, 0};
        reply.read(ids);
        logId = std::get<0>(ids);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to commit HostFW error {EVENT}: {ERROR}", "EVENT",
                   entry.getMessage(), "ERROR", e.what());
    }

    for (const auto fd : openedFds)
    {
        ::close(fd);
    }
    return logId;
}

} // namespace

std::vector<uint32_t> createChipOpErrorPELs(
    const chipop::ChipOpError& err, targeting::TargetHandle chip,
    const std::string& event, [[maybe_unused]] uint32_t cmdClass,
    [[maybe_unused]] uint32_t cmdType,
    [[maybe_unused]] const std::filesystem::path& dumpPath)
{
    auto bus = sdbusplus::bus::new_default();
    std::vector<uint32_t> logIds;

    auto native = std::static_pointer_cast<errl::ErrlHandle>(err.native);
    if (native)
    {
        for (const auto& entry : native->getEntries())
        {
            if (auto logId = commitEntry(bus, *entry); logId != 0)
            {
                logIds.push_back(logId);
            }
        }
        return logIds;
    }

    std::unordered_map<std::string, std::string> additionalData = {
        {"_PID", std::to_string(::getpid())},
        {"CHIP_OP_ERROR", err.what()},
        {"CHIP_POS", std::to_string(targeting::chipPos(chip))},
        {"CHIP_PATH", targeting::debugPath(chip)},
    };
    auto entry = errl::ErrlEntry(event, errl::Level::Error,
                                 std::move(additionalData));
    if (auto logId = commitEntry(bus, entry); logId != 0)
    {
        logIds.push_back(logId);
    }
    return logIds;
}

std::tuple<uint32_t, std::string> getPelInfo(uint32_t logId)
{
    constexpr auto entryInterface = "xyz.openbmc_project.Logging.Entry";
    constexpr auto opEntryInterface = "org.open_power.Logging.PEL.Entry";
    auto path = std::string(loggingObjectPath) + "/entry/" +
                std::to_string(logId);

    auto bus = sdbusplus::bus::new_default();
    auto service = util::getService(bus, entryInterface, path);

    auto method = bus.new_method_call(
        service.c_str(), path.c_str(), "org.freedesktop.DBus.Properties",
        "Get");
    method.append(opEntryInterface, "PlatformLogID");
    auto reply = bus.call(method);
    auto value = reply.unpack<std::variant<uint32_t, std::string>>();
    auto pelId = std::get<uint32_t>(value);

    method = bus.new_method_call(
        service.c_str(), path.c_str(), "org.freedesktop.DBus.Properties",
        "Get");
    method.append(entryInterface, "EventId");
    reply = bus.call(method);
    reply.read(value);

    std::istringstream stream(std::get<std::string>(value));
    std::string src;
    stream >> src;
    return {pelId, src};
}

} // namespace openpower::dump::phal::error

#include "dump_monitor.hpp"

#include "dump_utils.hpp"

#include <phosphor-logging/lg2.hpp>

namespace openpower::dump
{

constexpr auto dumpOutPath = "/var/lib/phosphor-debug-collector/opdump";
constexpr auto dumpStatusFailed =
    "xyz.openbmc_project.Common.Progress.OperationStatus.Failed";

void DumpMonitor::executeCollectionScript(
    const sdbusplus::message::object_path& path, const PropertyMap& properties)
{
    std::vector<std::string> args = {"opdreport"};
    auto dumpIdStr = extractIdFromPath(path);
    if (dumpIdStr == "")
    {
        lg2::error("Failed to extract dump id from path {PATH}", "PATH", path);
        updateProgressStatus(path, dumpStatusFailed);
        return;
    }

    uint32_t dumpId = std::strtoul(dumpIdStr.c_str(), nullptr, 16);
    int dumpType = getDumpTypeFromId(dumpId);
    std::filesystem::path dumpPath = std::filesystem::path(dumpOutPath) /
                                     dumpIdStr;

    // Add type, ID, and dump path to args
    args.push_back("-t");
    args.push_back(std::to_string(dumpType));
    args.push_back("-i");
    args.push_back(dumpIdStr);
    args.push_back("-d");
    args.push_back(dumpPath.string());

    // Optionally add ErrorLogId and FailingUnitId
    auto errorLogIdIt = properties.find("ErrorLogId");
    if (errorLogIdIt != properties.end())
    {
        uint32_t errorLogId = std::get<uint32_t>(errorLogIdIt->second);
        args.push_back("-e");
        args.push_back(std::to_string(errorLogId));
    }

    auto failingUnitIdIt = properties.find("FailingUnitId");
    if (failingUnitIdIt != properties.end())
    {
        uint32_t failingUnitId = std::get<uint32_t>(failingUnitIdIt->second);
        args.push_back("-f");
        args.push_back(std::to_string(failingUnitId));
    }

    std::vector<char*> argv;
    for (auto& arg : args)
    {
        argv.push_back(arg.data());
    }

    argv.push_back(nullptr);
    pid_t pid = fork();
    if (pid == -1)
    {
        lg2::error("Failed to fork, cannot collect dump, {ERRRNO}", "ERRNO",
                   errno);
        updateProgressStatus(path, dumpStatusFailed);
        exit(EXIT_FAILURE); // Exit explicitly with failure status
    }
    else if (pid == 0)
    {
        // Child process
        execvp("opdreport", argv.data());
        perror("execvp");   // execvp only returns on error
        updateProgressStatus(path, dumpStatusFailed);
        exit(EXIT_FAILURE); // Exit explicitly with failure status
    }
    else
    {
        // Parent process: wait for the child to terminate
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
        {
            int exit_status = WEXITSTATUS(status);

            if (exit_status != 0)
            {
                // Handle failure
                lg2::error("Dump failed updating status {PATH}", "PATH", path);
                updateProgressStatus(path, dumpStatusFailed);
            }
        }
    }
}

void DumpMonitor::handleDBusSignal(sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path objectPath;
    InterfaceMap interfaces;

    msg.read(objectPath, interfaces);

    lg2::info("Signal received at PATH: ", "PATH", objectPath.str);

    // There can be new a entry created after the completion of the collection
    // with completed status, so pick only entries with InProgress status.
    if (isInProgress(interfaces))
    {
        for (const auto& interfaceName : monitoredInterfaces)
        {
            auto it = interfaces.find(interfaceName);
            if (it != interfaces.end())
            {
                lg2::info("An entry created, collecting a new dump {DUMP}",
                          "DUMP", interfaceName);
                executeCollectionScript(objectPath, it->second);
            }
        }
    }
}

std::string
    DumpMonitor::extractIdFromPath(const sdbusplus::message::object_path& path)
{
    std::string pathStr = path.str;
    size_t lastSlash = pathStr.rfind('/');
    if (lastSlash != std::string::npos && lastSlash + 1 < pathStr.size())
    {
        return pathStr.substr(lastSlash + 1);
    }
    return {};
}

void DumpMonitor::updateProgressStatus(const std::string& path,
                                       const std::string& status)
{
    auto bus = sdbusplus::bus::new_default();
    const std::string interfaceName = "xyz.openbmc_project.Common.Progress";
    const std::string propertyName = "Status";
    const std::string serviceName = "xyz.openbmc_project.Dump.Manager";

    try
    {
        auto statusVariant = std::variant<std::string>(status);
        util::setProperty<std::variant<std::string>>(
            interfaceName, propertyName, path, bus, statusVariant);
        lg2::info("Status updated successfully to {STATUS} {PATH}", "STATUS",
                  status, "PATH", path);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Failed to update status {STATUS} {PATH} {ERROR}", "STATUS",
                   status, "PATH", path, "ERROR", e);
    }
}

} // namespace openpower::dump

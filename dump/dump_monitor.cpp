#include "dump_monitor.hpp"

#include "dump_utils.hpp"
#include "sbe_consts.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdeventplus/exception.hpp>
#include <sdeventplus/source/base.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Dump/Create/error.hpp>

#include <format>

namespace openpower
{
namespace dump
{

constexpr auto dumpOutPath = "/var/lib/phosphor-debug-collector/opdump";

void DumpMonitor::executeCollectionScript(
    const sdbusplus::message::object_path& path,
    const std::map<std::string, std::variant<uint32_t, std::string>>&
        properties)
{
    std::vector<std::string> args = {"opdreport"};
    auto dumpIdStr = extractIdFromPath(path);

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
    }
    else if (pid == 0)
    {
        // Child process
        execvp("opdreport", argv.data());
        perror("execvp");   // execvp only returns on error
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
                updateProgressStatus(
                    path,
                    "xyz.openbmc_project.Common.Progress.OperationStatus.Failed");
            }
        }
    }
}

} // namespace dump
} // namespace openpower

#pragma once

#include "dump_utils.hpp"
#include "sbe_consts.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>

#include <iostream>
#include <string>
#include <variant>

namespace openpower::dump
{

/**
 * @class DumpMonitor
 * @brief Monitors DBus signals for dump creation and handles them.
 */
class DumpMonitor
{
  public:
    /**
     * @brief Constructor for DumpMonitor.
     * Initializes the DBus connection and signal match for monitoring dump
     * creation.
     */
    DumpMonitor() :
        bus(sdbusplus::bus::new_default()),
        match(std::make_unique<sdbusplus::bus::match::match>(
            bus,
            "type='signal',"
            "interface='org.freedesktop.DBus.ObjectManager',"
            "member='InterfacesAdded',"
            "path_namespace='/xyz/openbmc_project/dump',"
            "sender='xyz.openbmc_project.Dump.Manager'",
            [this](sdbusplus::message::message& msg) {
        handleDBusSignal(msg);
    }))
    {
        // setupDBusMatch();
        monitoredInterfaces = {"com.ibm.Dump.Entry.Hardware",
                               "com.ibm.Dump.Entry.Hostboot",
                               "com.ibm.Dump.Entry.SBE"};
    }

    /**
     * @brief Runs the monitor to continuously listen for DBus signals.
     */
    void run()
    {
        while (true)
        {
            bus.process_discard();
            bus.wait();
        }
    }

  private:
    sdbusplus::bus::bus bus;
    std::vector<std::string> monitoredInterfaces;
    std::unique_ptr<sdbusplus::bus::match::match> match;

    /**
     * @brief Handles the received DBus signal for dump creation.
     * @param[in] msg - The DBus message received.
     */
    void handleDBusSignal(sdbusplus::message::message& msg)
    {
        sdbusplus::message::object_path objectPath;
        std::map<std::string,
                 std::map<std::string, std::variant<uint32_t, std::string>>>
            interfaces;

        msg.read(objectPath, interfaces);

        lg2::info("Signal received at PATH: ", "PATH", objectPath.str);

        if (isInProgress(interfaces))
        {
            for (const auto& interfaceName : monitoredInterfaces)
            {
                auto it = interfaces.find(interfaceName);
                if (it != interfaces.end())
                {
                    lg2::info("New dump entry created {DUMP}", "DUMP",
                              interfaceName);
                    executeCollectionScript(objectPath, it->second);
                }
            }
        }
    }

    /**
     * @brief Checks if the dump creation is in progress.
     * @param[in] interfaces - The map of interfaces and their properties.
     * @return True if the dump is in progress, false otherwise.
     */
    bool isInProgress(
        const std::map<
            std::string,
            std::map<std::string, std::variant<uint32_t, std::string>>>&
            interfaces)
    {
        auto progressIt =
            interfaces.find("xyz.openbmc_project.Common.Progress");
        if (progressIt != interfaces.end())
        {
            auto statusIt = progressIt->second.find("Status");
            if (statusIt != progressIt->second.end())
            {
                std::string status = std::get<std::string>(statusIt->second);
                return status ==
                       "xyz.openbmc_project.Common.Progress.OperationStatus.InProgress";
            }
        }
        return false;
    }

    /**
     * @brief Executes the script to collect the dump.
     * @param[in] path - The object path of the dump entry.
     * @param[in] properties - The properties of the dump entry.
     */
    void executeCollectionScript(
        const sdbusplus::message::object_path& path,
        const std::map<std::string, std::variant<uint32_t, std::string>>&
            properties);

    /**
     * @brief Extracts the dump ID from the object path.
     * @param[in] path - The object path of the dump entry.
     * @return The dump ID as string.
     */
    std::string extractIdFromPath(const sdbusplus::message::object_path& path)
    {
        std::string pathStr = path.str;
        size_t lastSlash = pathStr.rfind('/');
        if (lastSlash != std::string::npos && lastSlash + 1 < pathStr.size())
        {
            return pathStr.substr(lastSlash + 1);
        }
        return "00000000";
    }

    /**
     * @brief Updates the progress status of the dump.
     * @param[in] path - The object path of the dump entry.
     * @param[in] status - The status to be set.
     */
    void updateProgressStatus(const std::string& path,
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
            lg2::info("Status updated successfully to {STATUS} {PATH}",
                      "STATUS", status, "PATH", path);
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error("Failed to update status {STATUS} {PATH} {ERROR}",
                       "STATUS", status, "PATH", path, "ERROR", e);
        }
    }

    /**
     * @brief Gets the dump type from the dump ID.
     * @param[in] id - The dump ID.
     * @return The dump type.
     */
    uint32_t getDumpTypeFromId(uint32_t id)
    {
        using namespace openpower::dump::SBE;
        uint8_t type = (id >> 28) & 0xF;
        if (type == 0)
        {
            return SBE_DUMP_TYPE_HARDWARE;
        }
        else if (type == 2)
        {
            return SBE_DUMP_TYPE_HOSTBOOT;
        }
        else if (type == 3)
        {
            return SBE_DUMP_TYPE_SBE;
        }
        return 0;
    }
};

} // namespace openpower::dump

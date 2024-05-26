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

using PropertyMap = std::map<std::string, std::variant<uint32_t, std::string>>;
using InterfaceMap = std::map<std::string, PropertyMap>;

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
        match(std::make_unique<sdbusplus::bus::match_t>(
            bus,
            sdbusplus::bus::match::rules::type::signal() +
                sdbusplus::bus::match::rules::interface(
                    "org.freedesktop.DBus.ObjectManager") +
                sdbusplus::bus::match::rules::member("InterfacesAdded") +
                sdbusplus::bus::match::rules::path_namespace(
                    "/xyz/openbmc_project/dump") +
                sdbusplus::bus::match::rules::sender(
                    "xyz.openbmc_project.Dump.Manager"),
            [this](sdbusplus::message_t& msg) { handleDBusSignal(msg); }))
    {}

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
    std::vector<std::string> monitoredInterfaces = {
        "com.ibm.Dump.Entry.Hardware",
        "com.ibm.Dump.Entry.Hostboot",
        "com.ibm.Dump.Entry.SBE",
        "xyz.openbmc_project.Dump.Entry.System",
    };
    std::unique_ptr<sdbusplus::bus::match::match> match;

    /**
     * @brief Handles the received DBus signal for dump creation.
     * @param[in] msg - The DBus message received.
     */
    void handleDBusSignal(sdbusplus::message::message& msg);

    /**
     * @brief Checks if the dump creation is in progress.
     * @param[in] interfaces - The map of interfaces and their properties.
     * @return True if the dump is in progress, false otherwise.
     */
    inline bool isInProgress(const InterfaceMap& interfaces)
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
    void executeCollectionScript(const sdbusplus::message::object_path& path,
                                 const PropertyMap& properties);

    /**
     * @brief Updates the progress status of the dump.
     * @param[in] path - The object path of the dump entry.
     * @param[in] status - The status to be set.
     */
    void updateProgressStatus(const std::string& path,
                              const std::string& status);

    /**
     * @brief Gets the dump type from the dump ID.
     * @param[in] id - The dump ID.
     * @return The dump type.
     */
    inline uint32_t getDumpTypeFromId(uint32_t id)
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

    inline void initiateDumpCollection(const std::string& path,
                                       const std::string& intf,
                                       const PropertyMap& properties)
    {
        if (intf == "xyz.openbmc_project.Dump.Entry.System")
        {
            // Find the SystemImpact property, if this property is not available
            // assume disruptive.
            auto systemImpactIt = properties.find("SystemImpact");
            if (systemImpactIt != properties.end() &&
                std::get<std::string>(systemImpactIt->second) !=
                    "xyz.openbmc_project.Dump.Entry.System.SystemImpact.Disruptive")
            {
                lg2::info("Ignoring non-disruptive system dump {PATH}", "PATH",
                          path);
                return;
            }
            startMpReboot(path);
        }
        else
        {
            executeCollectionScript(path, properties);
        }
    }

    /**
     * @brief Initiates a memory-preserving reboot by starting the
     *        appropriate systemd unit.
     */
    void startMpReboot(const sdbusplus::message::object_path& objectPath);
};

} // namespace openpower::dump

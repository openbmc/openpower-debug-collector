#pragma once

#include "dump_utils.hpp"
#include "sbe_consts.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/Common/Progress/common.hpp>
#include <xyz/openbmc_project/Dump/Entry/System/common.hpp>

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
        match(bus,
              sdbusplus::bus::match::rules::interfacesAdded(
                  "/xyz/openbmc_project/dump") +
                  sdbusplus::bus::match::rules::sender(
                      "xyz.openbmc_project.Dump.Manager"),
              [this](sdbusplus::message_t& msg) { handleDBusSignal(msg); })
    {}

    /**
     * @brief Runs the monitor to continuously listen for DBus signals.
     */
    void run()
    {
        bus.process_loop();
    }

  private:
    /* @brief sdbusplus handler for a bus to use */
    sdbusplus::bus_t bus;

    /* @brief Monitores dump interfaces */
    const std::vector<std::string> monitoredInterfaces = {
        "com.ibm.Dump.Entry.Hardware", "com.ibm.Dump.Entry.Hostboot",
        "com.ibm.Dump.Entry.SBE", "xyz.openbmc_project.Dump.Entry.System"};

    /* @brief InterfaceAdded match */
    sdbusplus::bus::match_t match;

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
        using namespace sdbusplus::common::xyz::openbmc_project::common;
        auto progressIt = interfaces.find(Progress::interface);
        if (progressIt != interfaces.end())
        {
            auto statusIt = progressIt->second.find("Status");
            if (statusIt != progressIt->second.end())
            {
                std::string status = std::get<std::string>(statusIt->second);
                return status == Progress::convertOperationStatusToString(
                                     Progress::OperationStatus::InProgress);
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
        else if (type == 4)
        {
            return SBE_DUMP_TYPE_MSBE;
        }
        return 0;
    }

    /**
     * @brief Initiates the dump collection process based on the given interface
     *        and properties.
     *
     * @param[in] path The object path of the dump entry.
     * @param[in] intf The interface type of the dump entry.
     * @param[in] properties The properties of the dump entry.
     */
    void initiateDumpCollection(const std::string& path,
                                const std::string& intf,
                                const PropertyMap& properties);

    /**
     * @brief Initiates a memory-preserving reboot by starting the
     *        appropriate systemd unit.
     */
    void startMpReboot(const sdbusplus::message::object_path& objectPath);
};

} // namespace openpower::dump

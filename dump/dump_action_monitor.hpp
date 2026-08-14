#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openpower::dump
{

/** @class DumpActionMonitor
 *  @brief Creates IBM PELs for successful dump lifecycle actions.
 */
class DumpActionMonitor
{
  public:
    /** @brief Attach dump lifecycle signal matches to an existing bus. */
    DumpActionMonitor(sdbusplus::bus_t& bus, const sdeventplus::Event& event);

    DumpActionMonitor() = delete;
    DumpActionMonitor(const DumpActionMonitor&) = delete;
    DumpActionMonitor& operator=(const DumpActionMonitor&) = delete;
    DumpActionMonitor(DumpActionMonitor&&) = delete;
    DumpActionMonitor& operator=(DumpActionMonitor&&) = delete;
    ~DumpActionMonitor() = default;

  private:
    /** @brief Process a dump entry InterfacesRemoved signal. */
    void handleInterfacesRemoved(sdbusplus::message_t& msg);

    /** @brief Process a dump entry PropertiesChanged signal. */
    void handlePropertiesChanged(sdbusplus::message_t& msg);

    /** @brief Return all interfaces currently hosted on an object. */
    std::vector<std::string> getInterfaces(const std::string& path);

    /** @brief Return the unique owner of the dump-manager service. */
    std::optional<std::string> getDumpManagerOwner();

    /** @brief Verify and create PELs for queued removal signals. */
    void processPendingDeletions();

    /** @brief Return the user-facing dump type for an interface list. */
    static std::optional<std::string_view> getDumpType(
        const std::vector<std::string>& interfaces);

    /** @brief Create the IBM informational PEL for a dump action. */
    void createPEL(const sdbusplus::object_path& path,
                   std::string_view dumpType, std::string_view event);

    struct PendingDeletion
    {
        sdbusplus::object_path path;
        std::string dumpType;
        std::string sender;
    };

    using DeletionTimer =
        sdeventplus::utility::Timer<sdeventplus::ClockId::Monotonic>;

    sdbusplus::bus_t& bus;
    sdbusplus::match interfacesRemovedMatch;
    sdbusplus::match propertiesChangedMatch;
    std::map<std::string, PendingDeletion> pendingDeletions;
    DeletionTimer deletionTimer;
};

} // namespace openpower::dump

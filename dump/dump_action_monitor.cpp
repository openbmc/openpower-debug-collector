#include "dump_action_monitor.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>
#include <variant>

namespace openpower::dump
{
namespace
{
constexpr auto dumpRoot = "/xyz/openbmc_project/dump";
constexpr auto dumpManagerService = "xyz.openbmc_project.Dump.Manager";
constexpr auto dumpEntryInterface = "xyz.openbmc_project.Dump.Entry";

constexpr auto mapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperInterface = "xyz.openbmc_project.ObjectMapper";

constexpr auto dbusService = "org.freedesktop.DBus";
constexpr auto dbusPath = "/org/freedesktop/DBus";
constexpr auto dbusInterface = "org.freedesktop.DBus";

constexpr auto loggingService = "xyz.openbmc_project.Logging";
constexpr auto loggingPath = "/xyz/openbmc_project/logging";
constexpr auto loggingCreateInterface = "xyz.openbmc_project.Logging.Create";
constexpr auto informationalSeverity =
    "xyz.openbmc_project.Logging.Entry.Level.Informational";
constexpr auto dumpDeletedEvent = "xyz.openbmc_project.Dump.Error.Invalidate";
constexpr auto dumpOffloadedEvent = "xyz.openbmc_project.Dump.Error.Offload";
constexpr auto deletionVerificationDelay = std::chrono::seconds(1);

constexpr std::array<std::pair<std::string_view, std::string_view>, 6>
    dumpTypeInterfaces = {
        std::pair{"xyz.openbmc_project.Dump.Entry.BMC", "BMC Dump"},
        std::pair{"xyz.openbmc_project.Dump.Entry.System", "System Dump"},
        std::pair{"com.ibm.Dump.Entry.Resource", "Resource Dump"},
        std::pair{"com.ibm.Dump.Entry.Hostboot", "Hostboot Dump"},
        std::pair{"com.ibm.Dump.Entry.Hardware", "Hardware Dump"},
        std::pair{"com.ibm.Dump.Entry.SBE", "SBE Dump"},
};
} // namespace

DumpActionMonitor::DumpActionMonitor(sdbusplus::bus_t& bus,
                                     const sdeventplus::Event& event) :
    bus(bus),
    interfacesRemovedMatch(
        bus,
        sdbusplus::match_rules::interfacesRemoved(dumpRoot) +
            sdbusplus::match_rules::sender(dumpManagerService),
        [this](sdbusplus::message_t& msg) { handleInterfacesRemoved(msg); }),
    propertiesChangedMatch(
        bus,
        sdbusplus::match_rules::propertiesChangedNamespace(dumpRoot,
                                                           dumpEntryInterface) +
            sdbusplus::match_rules::sender(dumpManagerService),
        [this](sdbusplus::message_t& msg) { handlePropertiesChanged(msg); }),
    deletionTimer(event, [this](auto&) { processPendingDeletions(); })
{}

void DumpActionMonitor::handleInterfacesRemoved(sdbusplus::message_t& msg)
{
    sdbusplus::object_path path;
    std::vector<std::string> interfaces;

    try
    {
        msg.read(path, interfaces);
        auto dumpType = getDumpType(interfaces);
        if (!dumpType)
        {
            return;
        }

        // InterfacesRemoved contains the deleted object path and its former
        // type-specific interfaces, so no lookup of the removed object is
        // required.
        pendingDeletions.insert_or_assign(
            path.str,
            PendingDeletion{path, std::string(*dumpType), msg.get_sender()});
        deletionTimer.restartOnce(deletionVerificationDelay);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to process deleted dump {PATH}: {ERROR}", "PATH",
                   path, "ERROR", e);
    }
}

void DumpActionMonitor::handlePropertiesChanged(sdbusplus::message_t& msg)
{
    using Property = std::variant<bool, uint64_t, std::string>;

    std::string interface;
    std::map<std::string, Property> properties;
    std::vector<std::string> invalidated;

    try
    {
        msg.read(interface, properties, invalidated);
        auto property = properties.find("Offloaded");
        if (property == properties.end())
        {
            return;
        }

        const auto* offloaded = std::get_if<bool>(&property->second);
        if (offloaded == nullptr || !*offloaded)
        {
            return;
        }

        sdbusplus::object_path path(msg.get_path());
        auto dumpType = getDumpType(getInterfaces(path.str));
        if (!dumpType)
        {
            return;
        }

        createPEL(path, *dumpType, dumpOffloadedEvent);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to process offloaded dump {PATH}: {ERROR}", "PATH",
                   msg.get_path(), "ERROR", e);
    }
}

std::vector<std::string> DumpActionMonitor::getInterfaces(
    const std::string& path)
{
    auto method = bus.new_method_call(mapperService, mapperPath,
                                      mapperInterface, "GetObject");
    method.append(path, std::vector<std::string>{});

    auto response =
        bus.call(method)
            .unpack<std::map<std::string, std::vector<std::string>>>();
    auto service = response.find(dumpManagerService);
    if (service != response.end())
    {
        return service->second;
    }

    if (response.empty())
    {
        throw std::runtime_error("No services found for dump entry");
    }

    return response.begin()->second;
}

std::optional<std::string> DumpActionMonitor::getDumpManagerOwner()
{
    try
    {
        auto method = bus.new_method_call(dbusService, dbusPath, dbusInterface,
                                          "GetNameOwner");
        method.append(dumpManagerService);
        return bus.call(method).unpack<std::string>();
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::info("Dump manager has no D-Bus owner: {ERROR}", "ERROR", e);
        return std::nullopt;
    }
}

void DumpActionMonitor::processPendingDeletions()
{
    auto owner = getDumpManagerOwner();

    for (const auto& [path, deletion] : pendingDeletions)
    {
        // Object destructors also emit InterfacesRemoved during manager
        // shutdown. Only log deletion while the original signal sender still
        // owns the well-known dump-manager name.
        if (!owner || *owner != deletion.sender)
        {
            lg2::info("Ignoring dump removal during manager shutdown: {PATH}",
                      "PATH", path);
            continue;
        }

        try
        {
            createPEL(deletion.path, deletion.dumpType, dumpDeletedEvent);
        }
        catch (const std::exception& e)
        {
            lg2::error("Failed to create deleted dump PEL {PATH}: {ERROR}",
                       "PATH", path, "ERROR", e);
        }
    }

    pendingDeletions.clear();
}

std::optional<std::string_view> DumpActionMonitor::getDumpType(
    const std::vector<std::string>& interfaces)
{
    for (const auto& [interface, dumpType] : dumpTypeInterfaces)
    {
        if (std::ranges::find(interfaces, interface) != interfaces.end())
        {
            return dumpType;
        }
    }

    return std::nullopt;
}

void DumpActionMonitor::createPEL(const sdbusplus::object_path& path,
                                  std::string_view dumpType,
                                  std::string_view event)
{
    std::map<std::string, std::string> additionalData = {
        {"Dump ID", path.filename()},
        {"Dump Type", std::string(dumpType)},
        {"Dump Entry", path.str},
    };

    auto method = bus.new_method_call(loggingService, loggingPath,
                                      loggingCreateInterface, "Create");
    method.append(std::string(event), std::string(informationalSeverity),
                  additionalData);
    bus.call(method);

    lg2::info("Created PEL for dump action {EVENT}: {PATH}", "EVENT", event,
              "PATH", path);
}

} // namespace openpower::dump

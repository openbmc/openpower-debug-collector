// SPDX-License-Identifier: Apache-2.0
#include "metadata.hpp"

#include <unistd.h>

#include <sdbusplus/bus.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <variant>

namespace openpower::dump::header
{
namespace
{

constexpr auto propertiesInterface = "org.freedesktop.DBus.Properties";
constexpr auto inventoryService = "xyz.openbmc_project.Inventory.Manager";
constexpr auto systemInventoryPath = "/xyz/openbmc_project/inventory/system";
constexpr auto bmcInventoryPath =
    "/xyz/openbmc_project/inventory/system/chassis/motherboard";
constexpr auto assetInterface = "xyz.openbmc_project.Inventory.Decorator.Asset";
constexpr auto dumpManagerService = "xyz.openbmc_project.Dump.Manager";
constexpr auto originatorInterface = "xyz.openbmc_project.Common.OriginatedBy";
constexpr auto loggingService = "xyz.openbmc_project.Logging";
constexpr auto pelEntryInterface = "org.open_power.Logging.PEL.Entry";

template <typename T>
std::optional<T> readProperty(sdbusplus::bus_t& bus, const char* service,
                              const std::string& path, const char* interface,
                              const char* property)
{
    try
    {
        auto method = bus.new_method_call(service, path.c_str(),
                                          propertiesInterface, "Get");
        method.append(interface, property);
        auto reply = bus.call(method);
        std::variant<T> value;
        reply.read(value);
        return std::get<T>(value);
    }
    catch (const std::exception& error)
    {
        std::cerr << "op-dump-packager: could not read " << property << " at "
                  << path << ": " << error.what() << '\n';
        return std::nullopt;
    }
}

std::optional<Originator> parseOriginator(const std::string& value)
{
    const auto separator = value.rfind('.');
    const auto name =
        separator == std::string::npos ? value : value.substr(separator + 1);
    if (name == "Client")
    {
        return Originator::client;
    }
    if (name == "Internal")
    {
        return Originator::internal;
    }
    if (name == "SupportingService")
    {
        return Originator::supportingService;
    }
    std::cerr << "op-dump-packager: unsupported OriginatorType " << value
              << "; leaving it blank\n";
    return std::nullopt;
}

std::string getHostname()
{
    std::array<char, 256> hostname{};
    if (::gethostname(hostname.data(), hostname.size()) != 0)
    {
        return {};
    }
    hostname.back() = '\0';
    return hostname.data();
}

} // namespace

RawMetadata readMetadata(Profile profile, uint32_t dumpId,
                         const std::optional<std::string>& pelPath)
{
    RawMetadata result{};
    result.hostname = getHostname();

    try
    {
        auto bus = sdbusplus::bus::new_default();
        if (const auto model = readProperty<std::string>(
                bus, inventoryService, systemInventoryPath, assetInterface,
                "Model"))
        {
            result.model = *model;
        }
        if (const auto serial = readProperty<std::string>(
                bus, inventoryService, systemInventoryPath, assetInterface,
                "SerialNumber"))
        {
            result.systemSerial = *serial;
        }

        if (profile == Profile::system)
        {
            return result;
        }

        if (const auto bmcSerial = readProperty<std::string>(
                bus, inventoryService, bmcInventoryPath, assetInterface,
                "SerialNumber"))
        {
            result.bmcSerial = *bmcSerial;
        }

        const auto dumpPath =
            "/xyz/openbmc_project/dump/bmc/entry/" + std::to_string(dumpId);
        if (const auto originator = readProperty<std::string>(
                bus, dumpManagerService, dumpPath, originatorInterface,
                "OriginatorType"))
        {
            result.originator = parseOriginator(*originator);
        }
        if (const auto originatorId =
                readProperty<std::string>(bus, dumpManagerService, dumpPath,
                                          originatorInterface, "OriginatorId"))
        {
            result.originatorId = *originatorId;
        }

        if (pelPath.has_value())
        {
            result.eventLogId =
                readProperty<uint32_t>(bus, loggingService, *pelPath,
                                       pelEntryInterface, "PlatformLogID");
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "op-dump-packager: D-Bus metadata is unavailable: "
                  << error.what() << "; using fixed fallback values\n";
    }
    return result;
}

} // namespace openpower::dump::header

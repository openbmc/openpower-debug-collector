#pragma once

#include <sdbusplus/server.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <variant>
namespace openpower
{
namespace dump
{
namespace util
{

using DumpCreateParams =
    std::map<std::string, std::variant<std::string, uint64_t>>;

/**
 * @brief Get DBUS service for input interface via mapper call
 *
 * @param[in] bus -  DBUS Bus Object
 * @param[in] intf - DBUS Interface
 * @param[in] path - DBUS Object Path
 *
 * @return distinct dbus name for input interface/path
 **/
std::string getService(sdbusplus::bus::bus& bus, const std::string& intf,
                       const std::string& path);

/**
 * @brief Set the property value based on the inputs
 *
 * @param[in] interface - the interface the property is on
 * @param[in] propertName - the name of the property
 * @param[in] path - the D-Bus path
 * @param[in] service - the D-Bus service
 * @param[in] bus - the D-Bus object
 * @param[in] value - the value to set the property to
 */
template <typename T>
void setProperty(const std::string& interface, const std::string& propertyName,
                 const std::string& path, sdbusplus::bus::bus& bus,
                 const T& value)
{
    constexpr auto PROPERTY_INTF = "org.freedesktop.DBus.Properties";

    auto service = getService(bus, interface, path);
    auto method = bus.new_method_call(service.c_str(), path.c_str(),
                                      PROPERTY_INTF, "Set");
    method.append(interface, propertyName, value);
    auto reply = bus.call(method);
}

/** @brief create dump directories and add error log id
 *  @param dumpPath Directory for collecting dump
 *  @errorLogId Error log id associated with dump
 */
void prepareCollection(const std::filesystem::path& dumpPath,
                       const std::string& errorLogId);

} // namespace util
} // namespace dump
} // namespace openpower

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

constexpr auto SBE_DUMP_TIMEOUT = 4 * 60; // Timeout in seconds

using DumpCreateParams =
    std::map<std::string, std::variant<std::string, uint64_t>>;

/** @struct DumpPtr
 * @brief a structure holding the data pointer
 * @details This is a RAII container for the dump data
 * returned by the SBE
 */
struct DumpDataPtr
{
  public:
    /** @brief Destructor for the object, free the allocated memory.
     */
    ~DumpDataPtr()
    {
        // The memory is allocated using malloc
        free(dataPtr);
    }
    /** @brief Returns the pointer to the data
     */
    uint8_t** getPtr()
    {
        return &dataPtr;
    }
    /** @brief Returns the stored data
     */
    uint8_t* getData()
    {
        return dataPtr;
    }

  private:
    /** The pointer to the data */
    uint8_t* dataPtr = nullptr;
};

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

/**
 * Request SBE dump from the dump manager
 *
 * Request SBE dump from the dump manager and register a monitor for observing
 * the dump progress.
 *
 * @param failingUnit The id of the proc containing failed SBE
 * @param eid Error log id associated with dump
 */
void requestSBEDump(const uint32_t failingUnit, const uint32_t eid);

} // namespace util
} // namespace dump
} // namespace openpower

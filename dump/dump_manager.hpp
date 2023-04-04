#pragma once

#include <com/ibm/Dump/Create/server.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Dump/Create/server.hpp>

#include <map>

namespace openpower
{
namespace dump
{
using DumpCreateParams =
    std::map<std::string, std::variant<std::string, uint64_t>>;

using CreateIface = sdbusplus::server::object::object<
    sdbusplus::com::ibm::Dump::server::Create,
    sdbusplus::xyz::openbmc_project::Dump::server::Create>;

/** @class Manager
 *  @brief Dump  manager class
 *  @details A concrete implementation for the
 *  xyz::openbmc_project::Dump::server::Create D-Bus API.
 */
class Manager : public CreateIface
{
  public:
    Manager() = delete;
    Manager(const Manager&) = default;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(Manager&&) = delete;
    virtual ~Manager() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path of the service.
     */
    Manager(sdbusplus::bus::bus& bus, const char* path) :
        CreateIface(bus, path), bus(bus)
    {}

    /** @brief Implementation for createDump
     *  Method to request a host dump when there is an error related to
     *  host hardware or firmware.
     *  @param[in] params - Parameters for creating the dump.
     *
     *  @return object_path - The object path of the new dump entry.
     */
    sdbusplus::message::object_path
        createDump(DumpCreateParams createParams) override;

  private:
    /** @brief Get the dump path from dump type
     *  @param[in] params - Parameters for creating the dump.
     *
     *  @return D-Bus path for creating dump
     */
    std::string extractDumpPath(DumpCreateParams& param);

    /** @brief sdbusplus DBus bus connection. */
    sdbusplus::bus::bus& bus;
};

} // namespace dump
} // namespace openpower

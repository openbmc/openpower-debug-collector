#pragma once

#include "dump_utils.hpp"

#include <com/ibm/Dump/Create/server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdeventplus/source/child.hpp>
#include <sdeventplus/source/event.hpp>
#include <xyz/openbmc_project/Dump/Create/server.hpp>

#include <filesystem>

namespace openpower
{
namespace dump
{

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
     *  @param[in] event - sd event handler.
     */
    Manager(sdbusplus::bus::bus& bus, const char* path,
            sdeventplus::Event& event) :
        CreateIface(bus, path, true),
        bus(bus), event(event)
    {}

    /** @brief Implementation for createDump
     *  Method to request a host dump when there is an error related to
     *  host hardware or firmware.
     *  @param[in] params - Parameters for creating the dump.
     *
     *  @return object_path - The object path of the new dump entry.
     */
    sdbusplus::message::object_path
        createDump(util::DumpCreateParams params) override;

  private:
    /** @brief sdbusplus DBus bus connection. */
    sdbusplus::bus::bus& bus;

    /** @brief sdbusplus Dump event loop */
    sdeventplus::Event& event;

    /** @brief SDEventPlus child pointer added to event loop */
    std::unique_ptr<sdeventplus::source::Child> childPtr = nullptr;
};

} // namespace dump
} // namespace openpower

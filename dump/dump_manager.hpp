#pragma once

#include <com/ibm/Dump/Create/server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdeventplus/source/child.hpp>
#include <sdeventplus/source/event.hpp>
#include <xyz/openbmc_project/Dump/Create/server.hpp>

#include <map>

namespace openpower
{
namespace dump
{
/* Need a custom deleter for freeing up sd_event */
struct EventDeleter
{
    void operator()(sd_event* event) const
    {
        event = sd_event_unref(event);
    }
};

using EventPtr = std::unique_ptr<sd_event, EventDeleter>;

using DumpCreateParams =
    std::map<std::string, std::variant<std::string, uint64_t>>;

/** @struct DumpParams
 *  @brief Parameters for dump
 */
struct DumpParams
{
    uint8_t dumpType;     // Type of the dump
    uint64_t eid;         // Eid associated with dump
    uint64_t failingUnit; // Unit failed
    uint32_t id;          // Dump id
};

using CreateIface = sdbusplus::server::object::object<
    sdbusplus::com::ibm::Dump::server::Create,
    sdbusplus::xyz::openbmc_project::Dump::server::Create>;
using ::sdeventplus::source::Child;

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
    Manager(sdbusplus::bus::bus& bus, const char* path, const EventPtr& event) :
        CreateIface(bus, path, true), bus(bus), eventLoop(event.get())
    {}

    /** @brief Implementation for createDump
     *  Method to request a host dump when there is an error related to
     *  host hardware or firmware.
     *  @param[in] params - Parameters for creating the dump.
     *
     *  @return object_path - The object path of the new dump entry.
     */
    sdbusplus::message::object_path createDump(DumpCreateParams) override;

  private:
    /** @brief Get the dump params from arguments
     *  @param[in] params - Parameters for creating the dump.
     *  @param[out] dparams - Dump parameter struct with values
     */
    void getParams(const DumpCreateParams& params, DumpParams& dparams);

    /** @brief Create the entry for the dump
     *  @param[out] dparams - Dump parameter struct with values
     */
    sdbusplus::message::object_path createDumpEntry(DumpParams& dparams);

    /** @brief sdbusplus DBus bus connection. */
    sdbusplus::bus::bus& bus;

    /** @brief sdbusplus Dump event loop */
    EventPtr eventLoop;

    /** @brief map of SDEventPlus child pointer added to event loop */
    std::map<pid_t, std::unique_ptr<Child>> childPtrMap;
};

} // namespace dump
} // namespace openpower
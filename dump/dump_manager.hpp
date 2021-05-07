#pragma once

#include <libpdbg.h>

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

using DumpCreateParams =
    std::map<std::string, std::variant<std::string, uint64_t>>;
using CreateIface = sdbusplus::server::object::object<
    sdbusplus::com::ibm::Dump::server::Create,
    sdbusplus::xyz::openbmc_project::Dump::server::Create>;

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
    uint8_t* dataPtr = NULL;
};

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
        createDump(DumpCreateParams params) override;

  private:
    /** @brief sdbusplus DBus bus connection. */
    sdbusplus::bus::bus& bus;

    /** @brief sdbusplus Dump event loop */
    sdeventplus::Event& event;

    /** @brief SDEventPlus child pointer added to event loop */
    std::unique_ptr<sdeventplus::source::Child> childPtr = nullptr;

    /** @brief Method to find whether a processor is master
     *  @param[in] proc - pdbg_target for processor target
     *
     *  @return bool - true if master processor else false.
     */
    bool isMasterProc(struct pdbg_target* proc) const;

    /** @brief The function to orchestrate dump collection from different
     *  SBEs
     *  @param type - Type of the dump
     *  @param id - A unique id assigned to dump to be collected
     *  @param errorLogId - Id of the error log associated with this collection
     *  @param failingUnit - Chip position of the failing unit
     */
    void collectDump(uint8_t type, uint32_t id, std::string errorLogId,
                     const uint64_t failingUnit);

    /** @brief The function to collect dump from SBE
     *  @param[in] proc - pdbg_target of the proc conating SBE to collect the
     * dump.
     *  @param[in] dumpPath - Path of directory to write the dump files.
     *  @param[in] id - Id of the dump
     *  @param[in] type - Type of the dump
     *  @param[in] clockState - State of the clock while collecting.
     *  @param[in] chipPos - Position of the chip
     *  @param[in] collectFastArray - 0: skip fast array collection 1: collect
     * fast array
     */
    void collectDumpFromSBE(struct pdbg_target* proc,
                            std::filesystem::path& dumpPath, uint32_t id,
                            uint8_t type, uint8_t clockState,
                            const uint8_t chipPos,
                            const uint8_t collectFastArray);
};

} // namespace dump
} // namespace openpower

#include "attributes_info.H"

#include <fmt/format.h>

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <watchdog_common.hpp>
#include <watchdog_logging.hpp>

#include <map>

namespace watchdog
{
namespace dump
{

using namespace phosphor::logging;

void transitionHost(const std::string& target)
{
    constexpr auto systemdService = "org.freedesktop.systemd1";
    constexpr auto systemdObjPath = "/org/freedesktop/systemd1";
    constexpr auto systemdInterface = "org.freedesktop.systemd1.Manager";

    auto bus = sdbusplus::bus::new_system();
    auto method = bus.new_method_call(systemdService, systemdObjPath,
                                      systemdInterface, "StartUnit");

    method.append(target); // target unit to start
    method.append("replace");

    bus.call_noreply(method); // start the service
}

bool isAutoRebootEnabled()
{
    constexpr auto settingsService = "xyz.openbmc_project.Settings";
    constexpr auto settingsPath =
        "/xyz/openbmc_project/control/host0/auto_reboot";
    constexpr auto settingsIntf = "org.freedesktop.DBus.Properties";
    constexpr auto rebootPolicy =
        "xyz.openbmc_project.Control.Boot.RebootPolicy";

    auto bus = sdbusplus::bus::new_system();
    auto method =
        bus.new_method_call(settingsService, settingsPath, settingsIntf, "Get");

    method.append(rebootPolicy);
    method.append("AutoReboot");

    bool autoReboot = false;
    try
    {
        auto reply = bus.call(method);
        std::variant<bool> result;
        reply.read(result);
        autoReboot = std::get<bool>(result);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Error in AutoReboot Get", entry("ERROR=%s", e.what()));
    }

    return autoReboot;
}

bool isPrimaryProc(struct pdbg_target* procTarget)
{
    ATTR_PROC_MASTER_TYPE_Type type;

    // Get processor type (Primary or Secondary)
    if (DT_GET_PROP(ATTR_PROC_MASTER_TYPE, procTarget, type))
    {
        log<level::ERR>("Attribute [ATTR_PROC_MASTER_TYPE] get failed");
        throw std::runtime_error(
            "Attribute [ATTR_PROC_MASTER_TYPE] get failed");
    }

    bool primaryProc = false;
    /* Attribute value 0 corresponds to primary processor */
    if (type == ENUM_ATTR_PROC_MASTER_TYPE_ACTING_MASTER)
    {
        primaryProc = true;
    }
    return primaryProc;
}

void setDevtreeEnv()
{
    // PDBG_DTB environment variable set to CEC device tree path
    static constexpr auto PDBG_DTB_PATH =
        "/var/lib/phosphor-software-manager/pnor/rw/DEVTREE";

    if (setenv("PDBG_DTB", PDBG_DTB_PATH, true))
    {
        log<level::ERR>(
            fmt::format("Failed to set PDBG_DTB: ({})", strerror(errno))
                .c_str());
        throw std::runtime_error("Failed to set PDBG_DTB");
    }
}

struct pdbg_target* getPibTarget(struct pdbg_target* procTarget)
{
    char path[16];
    sprintf(path, "/proc%d/pib", pdbg_target_index(procTarget));
    struct pdbg_target* pib = pdbg_target_from_path(nullptr, path);
    if (pib == nullptr)
    {
        log<level::ERR>(fmt::format("Failed to get PIB target for: {}",
                                    pdbg_target_path(procTarget))
                            .c_str());
        throw std::runtime_error("Failed to get PIB target");
    }

    // Probe the target
    if (pdbg_target_probe(pib) != PDBG_TARGET_ENABLED)
    {
        log<level::ERR>(fmt::format("Failed to enable PIB target: {}",
                                    pdbg_target_path(pib))
                            .c_str());
        throw std::runtime_error("Failed to enable PIB target");
    }
    return pib;
}

void pdbgInit()
{
    // PDBG_DTB environment variable set to CEC device tree path
    setDevtreeEnv();

    log<level::INFO>("pdbgInit(): Initialize pdbg targets");
    if (!pdbg_targets_init(nullptr))
    {
        log<level::ERR>("pdbg_targets_init failed");
        throw std::runtime_error("pdbg target initialization failed");
    }
}

} // namespace dump
} // namespace watchdog

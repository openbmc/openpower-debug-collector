#include <fmt/format.h>
extern "C"
{
#include <libpdbg.h>
#include <libpdbg_sbe.h>
}

#include <libphal.H>

#include <phosphor-logging/log.hpp>
#include <watchdog_common.hpp>
#include <watchdog_dbus.hpp>
#include <watchdog_handler.hpp>
#include <watchdog_logging.hpp>

namespace watchdog
{
namespace dump
{

using namespace phosphor::logging;

void triggerHostbootDump(const uint32_t timeout)
{
    constexpr auto HOST_STATE_DIAGNOSTIC_MODE =
        "obmc-host-diagnostic-mode@0.target";
    constexpr auto HOST_STATE_QUIESCE_TGT = "obmc-host-quiesce@0.target";

    // Put system into diagnostic mode
    transitionHost(HOST_STATE_DIAGNOSTIC_MODE);

    eventWatchdogTimeout(timeout);

    // Put system into quiesce state
    transitionHost(HOST_STATE_QUIESCE_TGT);
}

void handleSbeBootError(struct pdbg_target* procTarget, const uint32_t timeout)
{
    using namespace openpower::phal;

    sbeError_t sbeError;
    bool dumpIsRequired = false;

    try
    {
        // Capture FFDC information on primary processor
        sbeError = sbe::captureFFDC(procTarget);
    }
    catch (const std::exception& e)
    {
        // Failed to collect FFDC information
        log<level::ERR>(
            fmt::format("captureFFDC: Exception{}", e.what()).c_str());
        dumpIsRequired = true;
    }

    // event type
    std::string event;
    if ((sbeError.errType() == exception::SBE_FFDC_NO_DATA) ||
        (sbeError.errType() == exception::SBE_CMD_TIMEOUT) || (dumpIsRequired))
    {
        log<level::INFO>("No FFDC data");
        event = "org.open_power.Processor.Error.SbeBootTimeout";
        dumpIsRequired = true;
    }
    else
    {
        log<level::ERR>("SBE Boot failure");
        event = "org.open_power.Processor.Error.SbeBootFailure";
    }

    // Additional data
    std::map<std::string, std::string> additionalData;

    // SRC6 : [0:15] chip position
    uint32_t index = pdbg_target_index(procTarget);
    additionalData.emplace("SRC6", std::to_string(index << 16));
    additionalData.emplace("SBE_ERR_MSG", sbeError.what());

    // FFDC
    auto ffdc = std::vector<FFDCTuple>{};
    // get SBE ffdc file descriptor
    auto fd = sbeError.getFd();

    // Log error with additional ffdc if fd is valid
    if (fd > 0)
    {
        ffdc.push_back(
            std::make_tuple(sdbusplus::xyz::openbmc_project::Logging::server::
                                Create::FFDCFormat::Custom,
                            static_cast<uint8_t>(0xCB),
                            static_cast<uint8_t>(0x01), sbeError.getFd()));
    }

    auto pelId = createPel(event, additionalData, ffdc);

    if (dumpIsRequired)
    {
        try
        {
            using namespace openpower::phal;

            // Check SBE dump collection allowed
            bool dumpAllowed = sbe::isDumpAllowed(procTarget);
            if (!dumpAllowed)
            {
                // Possibly another collection in progress, skip dump collection
                log<level::INFO>("Another collection is in progress, skipping "
                                 "dump collection");
                return;
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>(
                fmt::format("Exception {} occurred", e.what()).c_str());
            return;
        }

        DumpParameters dumpParameters;
        dumpParameters.logId = pelId;
        dumpParameters.unitId = index;
        dumpParameters.timeout = timeout;
        dumpParameters.dumpType = DumpType::SBE;

        // will not return until dump is complete or timeout
        requestDump(dumpParameters);
    }
}

} // namespace dump
} // namespace watchdog

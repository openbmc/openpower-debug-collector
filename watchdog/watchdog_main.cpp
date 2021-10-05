#include <fmt/format.h>
extern "C"
{
#include <libpdbg.h>
}
#include <libpdbg.h>
#include <libpdbg_sbe.h>
#include <libphal.H>

#include <phosphor-logging/log.hpp>
#include <watchdog_common.hpp>
#include <watchdog_dbus.hpp>
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

// Create SBE dump
void createSBEDump(uint32_t /*const pelId*/)
{
    log<level::ERR>("Create SBE dump");
    // TODO: Implement SBE dump collection
}

void triggerCaptureFFDC(struct pdbg_target* procTarget)
{
    using namespace openpower::phal;

    log<level::INFO>("Initiate SBEDump collection");
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

    std::string eventType;
    auto ffdc = std::vector<FFDCTuple>{};              // any ffdc data?
    std::map<std::string, std::string> additionalData; // any additional data?

    if ((sbeError.errType() == exception::SBE_FFDC_NO_DATA) ||
        (sbeError.errType() == exception::SBE_CMD_TIMEOUT) || (dumpIsRequired))
    {
        log<level::INFO>("SBE dump required!");
        eventType = "org.open_power.Processor.Error.SbeBootTimeout";

        // Create SBE Dump type error log
        uint32_t pelId = createPel(eventType, additionalData, ffdc);

        // Create SBE dump
        createSBEDump(pelId);

        return;
    }

    // SRC6 : [0:15] chip position
    eventType = "org.open_power.Processor.Error.SbeBootFailure";
    uint32_t word6 = pdbg_target_index(procTarget);
    additionalData.emplace("SRC6", std::to_string(word6 << 16));

    // Create SBE Error with FFDC data.
    createPel(eventType, additionalData, ffdc);
}

} // namespace dump
} // namespace watchdog

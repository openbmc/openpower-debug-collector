extern "C"
{
#include <libpdbg_sbe.h>
}

#include "create_pel.hpp"
#include "dump_collect.hpp"
#include "sbe_type.hpp"

#include <libphal.H>
#include <phal_exception.H>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sbe_consts.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>

namespace openpower
{
namespace dump
{
namespace sbe_chipop
{
constexpr auto SBEFIFO_CMD_CLASS_INSTRUCTION = 0xA700;
constexpr auto SBEFIFO_CMD_CONTROL_INSN = 0x01;

void writeDumpFile(const std::filesystem::path& path, const uint32_t id,
                   const uint8_t clockState, const uint8_t chipPos,
                   std::string chipName, util::DumpDataPtr& dataPtr,
                   const uint32_t len)
{
    using namespace phosphor::logging;
    using namespace sdbusplus::xyz::openbmc_project::Common::Error;
    namespace fileError = sdbusplus::xyz::openbmc_project::Common::File::Error;

    // Filename format: <dump_id>.SbeDataClocks<On/Off>.node0.proc<number>
    std::stringstream ss;
    ss << std::setw(8) << std::setfill('0') << id;
    std::string clockStr = (clockState == SBE::SBE_CLOCK_ON) ? "On" : "Off";

    // Assuming only node0 is supported now
    auto filename = ss.str() + ".SbeDataClocks" + clockStr + ".node0." +
                    chipName + std::to_string(chipPos);

    std::filesystem::path dumpPath = path / filename;
    std::ofstream outfile{dumpPath, std::ios::out | std::ios::binary};
    if (!outfile.good())
    {
        using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
        using metadata = xyz::openbmc_project::Common::File::Open;
        // Unable to open the file for writing
        auto err = errno;
        log<level::ERR>(
            std::format(
                "Error opening file to write dump, errno({}), filepath({})",
                err, dumpPath.string())
                .c_str());
        report<Open>(metadata::ERRNO(err), metadata::PATH(dumpPath.c_str()));
        // Just return here, so that the dumps collected from other
        // SBEs can be packaged.
        return;
    }
    outfile.exceptions(std::ifstream::failbit | std::ifstream::badbit |
                       std::ifstream::eofbit);
    try
    {
        outfile.write(reinterpret_cast<char*>(dataPtr.getData()), len);
    }
    catch (std::ofstream::failure& oe)
    {
        using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
        using metadata = xyz::openbmc_project::Common::File::Write;
        log<level::ERR>(std::format("Failed to write to dump file, "
                                    "errorMsg({}), error({}), filepath({})",
                                    oe.what(), oe.code().value(),
                                    dumpPath.string())
                            .c_str());
        report<Write>(metadata::ERRNO(oe.code().value()),
                      metadata::PATH(dumpPath.c_str()));
        // Just return here so dumps collected from other SBEs can be
        // packaged.
    }
    outfile.close();
}

bool checkForFastArrayCollection(uint8_t clockState, uint8_t type,
                                 SBETypes sbeType, uint64_t chipPos,
                                 uint64_t failingUnit)
{
    return clockState == SBE::SBE_CLOCK_OFF &&
           (type == SBE::SBE_DUMP_TYPE_HOSTBOOT || sbeType == SBETypes::OCMB ||
            (type == SBE::SBE_DUMP_TYPE_HARDWARE && chipPos == failingUnit));
}

SBETypes getSBEType(struct pdbg_target* chip)
{
    if (is_ody_ocmb_chip(chip))
    {
        return SBETypes::OCMB;
    }
    return SBETypes::PROC;
}

void collectDumpFromSBE(struct pdbg_target* chip,
                        const std::filesystem::path& path, const uint32_t id,
                        const uint8_t type, const uint8_t clockState,
                        const uint64_t failingUnit)
{
    using namespace phosphor::logging;
    auto chipPos = pdbg_target_index(chip);
    SBETypes sbeType = getSBEType(chip);

    log<level::INFO>(std::format("Collect dump from ({})({}) path({}) id({}) "
                                 "type({}) clock({}) failingUnit({})",
                                 sbeTypeAttributes[sbeType].chipName, chipPos,
                                 path.string(), id, type, clockState,
                                 failingUnit)
                         .c_str());

    util::DumpDataPtr dataPtr;
    uint32_t len = 0;
    uint8_t collectFastArray = checkForFastArrayCollection(clockState, type,
                                                           sbeType, chipPos,
                                                           failingUnit)
                                   ? 1
                                   : 0;

    try
    {
        openpower::phal::sbe::getDump(chip, type, clockState, collectFastArray,
                                      dataPtr.getPtr(), &len);
    }
    catch (const openpower::phal::sbeError_t& sbeError)
    {
        if (sbeError.errType() ==
            openpower::phal::exception::SBE_CHIPOP_NOT_ALLOWED)
        {
            // SBE is not ready to accept chip-ops,
            // Skip the request, no additional error handling required.
            log<level::INFO>(std::format("Collect dump: Skipping ({}) dump({}) "
                                         "on ({})({}) clock state({})",
                                         sbeError.what(), type,
                                         sbeTypeAttributes[sbeType].chipName,
                                         chipPos, clockState)
                                 .c_str());
            return;
        }
        log<level::ERR>(
            std::format(
                "Error in collecting dump dump type({}), clockstate({}), ({}) "
                "position({}), collectFastArray({}) error({})",
                type, clockState, sbeTypeAttributes[sbeType].chipName, chipPos,
                collectFastArray, sbeError.what())
                .c_str());

        std::string event = sbeTypeAttributes[sbeType].chipOpFailure;
        auto dumpIsRequired = false;

        if (sbeError.errType() == openpower::phal::exception::SBE_CMD_TIMEOUT)
        {
            event = sbeTypeAttributes[sbeType].timeoutError;
            dumpIsRequired = true;
        }

        openpower::dump::pel::FFDCData pelAdditionalData;
        uint32_t cmd = SBE::SBEFIFO_CMD_CLASS_DUMP | SBE::SBEFIFO_CMD_GET_DUMP;

        pelAdditionalData.emplace_back("SRC6",
                                       std::to_string((chipPos << 16) | cmd));

        auto logId = openpower::dump::pel::createSbeErrorPEL(event, sbeError,
                                                             pelAdditionalData);
        if (dumpIsRequired)
        {
            // Request SBE Dump
            try
            {
                util::requestSBEDump(chipPos, logId, sbeType);
            }
            catch (const std::exception& e)
            {
                log<level::ERR>(
                    std::format("SBE Dump request failed, ({})({}) error({})",
                                sbeTypeAttributes[sbeType].chipName, chipPos,
                                e.what())
                        .c_str());
            }
        }
        return;
    }
    writeDumpFile(path, id, clockState, chipPos,
                  sbeTypeAttributes[sbeType].chipName, dataPtr, len);
}

void collectDump(const uint8_t type, const uint32_t id,
                 const uint64_t failingUnit, const std::filesystem::path& path)
{
    using namespace phosphor::logging;
    log<level::INFO>(
        std::format(
            "Dump collection started type({}) id({}) failingUnit({}), path({})",
            type, id, failingUnit, path.string())
            .c_str());
    struct pdbg_target* target = nullptr;
    auto failed = false;
    // Initialize PDBG
    openpower::phal::pdbg::init();

    std::vector<struct pdbg_target*> targetList;

    pdbg_for_each_class_target("proc", target)
    {
        auto index = pdbg_target_index(target);
        if (pdbg_target_probe(target) != PDBG_TARGET_ENABLED)
        {
            continue;
        }

        // if the dump type is hostboot then call stop instructions
        if ((type == openpower::dump::SBE::SBE_DUMP_TYPE_HOSTBOOT) &&
            openpower::phal::pdbg::isTgtFunctional(target))
        {
            try
            {
                openpower::phal::sbe::threadStopProc(target);
            }
            catch (const openpower::phal::sbeError_t& sbeError)
            {
                auto errType = sbeError.errType();
                // Create PEL only for valid SBE reported failures
                if (errType == openpower::phal::exception::SBE_CMD_FAILED)
                {
                    log<level::ERR>(
                        std::format("Stop instructions SBE command failed, "
                                    " on proc({}) error({}), a "
                                    "PELL will be logged",
                                    index, sbeError.what())
                            .c_str());
                    uint32_t cmd = SBEFIFO_CMD_CLASS_INSTRUCTION |
                                   SBEFIFO_CMD_CONTROL_INSN;
                    // To store additional data about ffdc.
                    openpower::dump::pel::FFDCData pelAdditionalData;
                    // SRC6 : [0:15] chip position
                    //        [16:23] command class,  [24:31] Type
                    pelAdditionalData.emplace_back(
                        "SRC6", std::to_string((index << 16) | cmd));

                    // Create error log.
                    openpower::dump::pel::createSbeErrorPEL(
                        "org.open_power.Processor.Error.SbeChipOpFailure",
                        sbeError, pelAdditionalData,
                        openpower::dump::pel::Severity::Informational);
                }
                else
                {
                    log<level::INFO>(
                        std::format(
                            "Stop instructions failed, "
                            " on proc({}) error({}) error type({})",
                            index, sbeError.what(),
                            static_cast<std::underlying_type_t<
                                openpower::phal::exception::ERR_TYPE>>(errType))
                            .c_str());
                }
            }
        }
        targetList.push_back(target);
        if (type == openpower::dump::SBE::SBE_DUMP_TYPE_HARDWARE)
        {
            struct pdbg_target* ocmbTarget;
            pdbg_for_each_target("ocmb", target, ocmbTarget)
            {
                if (pdbg_target_probe(ocmbTarget) != PDBG_TARGET_ENABLED)
                {
                    continue;
                }

                if (!is_ody_ocmb_chip(ocmbTarget))
                {
                    continue;
                }
                targetList.push_back(ocmbTarget);
            }
        }
    }

    std::vector<uint8_t> clockStates = {SBE::SBE_CLOCK_ON, SBE::SBE_CLOCK_OFF};
    for (auto cstate : clockStates)
    {
        // Performace dump need to collect only when clocks are ON
        if ((type == SBE::SBE_DUMP_TYPE_PERFORMANCE) &&
            (cstate != SBE::SBE_CLOCK_ON))
        {
            continue;
        }

        std::vector<pid_t> pidList;

        for (pdbg_target* dumpTarget : targetList)
        {
            pid_t pid = fork();
            if (pid < 0)
            {
                log<level::ERR>(
                    "Fork failed while starting hostboot dump collection");
                throw std::runtime_error(
                    "Fork failed while starting hostboot dump collection");
            }
            else if (pid == 0)
            {
                try
                {
                    collectDumpFromSBE(dumpTarget, path, id, type, cstate,
                                       failingUnit);
                }
                catch (const std::runtime_error& error)
                {
                    log<level::ERR>(
                        std::format(
                            "Failed to execute collection, errorMsg({})",
                            error.what())
                            .c_str());
                    std::exit(EXIT_FAILURE);
                }
                std::exit(EXIT_SUCCESS);
            }
            else
            {
                pidList.push_back(std::move(pid));
            }
        }
        for (auto& p : pidList)
        {
            auto status = 0;
            waitpid(p, &status, 0);
            if (WEXITSTATUS(status))
            {
                log<level::ERR>(
                    std::format("Dump collection failed, status({}) pid({})",
                                status, p)
                        .c_str());
                failed = true;
            }
        }
        // Exit if there is a critical failure and collection cannot continue
        // or if the dump collection folder is empty return failure
        if ((failed) || (std::filesystem::is_empty(path)))
        {
            log<level::ERR>("Failed to collect the dump");
            std::exit(EXIT_FAILURE);
        }
        log<level::INFO>(
            std::format("Dump collection completed for clock_state({})", cstate)
                .c_str());
    }
}
} // namespace sbe_chipop
} // namespace dump
} // namespace openpower

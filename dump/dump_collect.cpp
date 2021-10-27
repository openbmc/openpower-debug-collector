extern "C"
{
#include <libpdbg.h>
#include <libpdbg_sbe.h>
}

#include "dump_collect.hpp"
#include "sbe_consts.hpp"

#include <libphal.H>
#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/log.hpp>

#include <format>
#include <stdexcept>

namespace openpower
{
namespace dump
{
namespace sbe_chipop
{
using namespace phosphor::logging;
using namespace openpower::dump::SBE;

void SbeDumpCollector::collectDump(uint8_t type, uint32_t id,
                                   uint64_t failingUnit,
                                   const std::filesystem::path& path)
{
    log<level::INFO>(std::format("Starting dump collection: "
                                 "type({}) id({}) failingUnit({}), path({})",
                                 type, id, failingUnit, path.string())
                         .c_str());

    initializePdbg();

    std::vector<struct pdbg_target*> targets;

    struct pdbg_target* target = nullptr;
    pdbg_for_each_class_target("proc", target)
    {
        if (pdbg_target_probe(target) != PDBG_TARGET_ENABLED ||
            !openpower::phal::pdbg::isTgtFunctional(target))
        {
            continue;
        }

        targets.push_back(target);
    }

    std::vector<uint8_t> clockStates = {SBE_CLOCK_ON, SBE_CLOCK_OFF};
    for (auto cstate : clockStates)
    {
        auto futures = spawnDumpCollectionProcesses(type, id, path, failingUnit,
                                                    cstate, targets);

        // Wait for all asynchronous tasks to complete
        for (auto& future : futures)
        {
            future.wait();
        }
        log<level::INFO>(
            std::format("Dump collection completed for clock state({}):"
                        "type({}) id({}) failingUnit({}), path({})",
                        cstate, type, id, failingUnit, path.string())
                .c_str());
    }

    log<level::INFO>("Dump collection completed");
}

void SbeDumpCollector::initializePdbg()
{
    openpower::phal::pdbg::init();
}

std::vector<std::future<void>> SbeDumpCollector::spawnDumpCollectionProcesses(
    uint8_t type, uint32_t id, const std::filesystem::path& path,
    uint64_t failingUnit, uint8_t cstate,
    const std::vector<struct pdbg_target*>& targets)
{
    std::vector<std::future<void>> futures;

    for (auto target : targets)
    {
        if (pdbg_target_probe(target) != PDBG_TARGET_ENABLED ||
            !openpower::phal::pdbg::isTgtFunctional(target))
        {
            continue;
        }

        // Launch an asynchronous task instead of forking
        auto future =
            std::async(std::launch::async,
                       [this, target, path, id, type, cstate, failingUnit]() {
            this->collectDumpFromSBE(target, path, id, type, cstate,
                                     failingUnit);
        });

        futures.push_back(std::move(future));
    }

    return futures;
}

void SbeDumpCollector::collectDumpFromSBE(struct pdbg_target* chip,
                                          const std::filesystem::path& path,
                                          uint32_t id, uint8_t type,
                                          uint8_t clockState,
                                          uint64_t failingUnit)
{
    auto chipPos = pdbg_target_index(chip);
    log<level::INFO>(
        std::format("Collecting dump from proc({}): path({}) id({}) "
                    "type({}) clockState({}) failingUnit({})",
                    chipPos, path.string(), id, type, clockState, failingUnit)
            .c_str());
}

} // namespace sbe_chipop
} // namespace dump
} // namespace openpower

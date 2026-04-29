#include "error_iface.hpp"

namespace openpower::dump::phal::error
{

bool logChipOpError([[maybe_unused]] const chipop::ChipOpError& err,
                    [[maybe_unused]] targeting::TargetHandle chip,
                    [[maybe_unused]] uint32_t cmdClass,
                    [[maybe_unused]] uint32_t cmdType,
                    [[maybe_unused]] const std::filesystem::path& dumpPath)
{
    // Stub: Not implemented for next backend
    return false;
}

uint32_t createChipOpErrorPEL(
    [[maybe_unused]] const chipop::ChipOpError& err,
    [[maybe_unused]] targeting::TargetHandle chip,
    [[maybe_unused]] const std::string& event,
    [[maybe_unused]] const std::filesystem::path& dumpPath)
{
    // Stub: Not implemented for next backend
    return 0;
}

std::tuple<uint32_t, std::string> getPelInfo([[maybe_unused]] uint32_t logId)
{
    // Stub: Not implemented for next backend
    return {0, ""};
}

} // namespace openpower::dump::phal::error

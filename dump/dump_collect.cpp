#include "dump_collect.hpp"

#include <fmt/core.h>

#include <phosphor-logging/log.hpp>

#include <cstdint>
#include <filesystem>

namespace openpower
{
namespace dump
{
namespace sbe_chipop
{

void collectDump(const uint8_t type, const uint32_t id,
                 const uint64_t failingUnit, const std::filesystem::path& path)
{
    using namespace phosphor::logging;
    log<level::INFO>(
        fmt::format(
            "Dump collection started type({}) id({}) failingUnit({}), path({})",
            type, id, failingUnit, path.string())
            .c_str());
}
} // namespace sbe_chipop
} // namespace dump
} // namespace openpower

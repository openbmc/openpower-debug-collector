// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "dump_header.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>

namespace openpower::dump::header
{

class ArchiveTooLarge : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

struct PackageRequest
{
    Profile profile{};
    std::filesystem::path archive;
    std::filesystem::path output;
    std::optional<uint64_t> maximumArchiveSize;
    DumpIdentifier dumpId{};
    uint64_t epochSeconds{};
    Generation generation{};
    NormalizedMetadata metadata{};
};

void packageDump(const PackageRequest& request);

} // namespace openpower::dump::header

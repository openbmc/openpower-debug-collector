// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "dump_header.hpp"

#include <optional>
#include <string>

namespace openpower::dump::header
{

RawMetadata readMetadata(Profile profile, uint32_t dumpId,
                         const std::optional<std::string>& pelPath);

} // namespace openpower::dump::header

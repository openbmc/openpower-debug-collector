#include "sbe_type.hpp"

namespace openpower::dump
{

const std::map<SBETypes, SBEAttributes> sbeTypeAttributes = {
    {SBETypes::PROC,
     {"proc", "/xyz/openbmc_project/dump/sbe",
      "org.open_power.Processor.Error.SbeChipOpTimeout",
      "org.open_power.Processor.Error.SbeChipOpFailure"}}};

} // namespace openpower::dump

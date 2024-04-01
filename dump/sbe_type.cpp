#include "sbe_type.hpp"

namespace openpower::dump
{

const std::map<SBETypes, SBEAttributes> sbeTypeAttributes = {
    {SBETypes::PROC,
     {"proc", "/xyz/openbmc_project/dump/sbe",
      "org.open_power.Processor.Error.SbeChipOpTimeout",
      "org.open_power.Processor.Error.SbeChipOpFailure"}},
    {SBETypes::OCMB,
     {"ocmb", "/xyz/openbmc_project/dump/msbe",
      "org.open_power.OCMB.Error.SbeChipOpTimeout",
      "org.open_power.OCMB.Error.SbeChipOpFailure"}}};

} // namespace openpower::dump

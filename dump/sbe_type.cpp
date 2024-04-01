#include "sbe_type.hpp"

namespace openpower
{
namespace dump
{

std::map<SBETypes, SBEAttributes> sbeTypeAttributes = {

    {SBETypes::PROC,
     {"proc", "/xyz/openbmc_project/dump/sbe",
      "org.open_power.Processor.Error.SbeChipOpTimeout",
      "org.open_power.Processor.Error.SbeChipOpFailure"}},
    {SBETypes::OCMB,
     {"ocmb", "/xyz/openbmc_project/dump/msbe",
      "org.open_power.OCMB.Error.SbeChipOpTimeout",
      "org.open_power.OCMB.Error.SbeChipOpFailure"}}};

} // namespace dump
} // namespace openpower

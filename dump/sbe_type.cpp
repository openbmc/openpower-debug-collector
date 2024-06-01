#include "sbe_type.hpp"

namespace openpower::dump
{

const std::map<SBETypes, SBEAttributes> sbeTypeAttributes = {
    {SBETypes::PROC,
     {"proc", "com.ibm.Dump.Create.DumpType.SBE",
      "org.open_power.Processor.Error.SbeChipOpTimeout",
      "org.open_power.Processor.Error.SbeChipOpFailure",
      "org.open_power.Processor.Error.NoFffdc",
      "org.open_power.Processor.Error.SbeInternalFFDCData"}},
    {SBETypes::OCMB,
     {"ocmb", "com.ibm.Dump.Create.DumpType.MemoryBufferSBE",
      "org.open_power.OCMB.Error.SbeChipOpTimeout",
      "org.open_power.OCMB.Error.SbeChipOpFailure",
      "org.open_power.OCMB.Error.NoFffdc",
      "org.open_power.OCMB.Error.SbeInternalFFDCData"}}};

} // namespace openpower::dump

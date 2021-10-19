#include "sbe_type.hpp"

namespace openpower::dump
{

const std::map<SBETypes, SBEAttributes> sbeTypeAttributes = {
    {SBETypes::PROC,
     {"proc", "org.open_power.Processor.Error.SbeChipOpFailure"}}};

} // namespace openpower::dump

#include "sbe_type.hpp"

namespace openpower
{
namespace dump
{

std::map<SBETypes, SBEAttributes> sbeTypeAttributes = {
    {SBETypes::PROC,
     {"proc", "org.open_power.Processor.Error.SbeChipOpFailure"}}};

} // namespace dump
} // namespace openpower

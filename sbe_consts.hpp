#pragma once
namespace openpower
{
namespace dump
{
namespace SBE
{
// Dump type to the sbe_dump chipop
constexpr auto SBE_DUMP_TYPE_HOSTBOOT = 0x5;

// Clock state requested
// Collect the dump with clocks on
constexpr auto SBE_CLOCK_ON = 0x1;

// Collect the dumps with clock off
constexpr auto SBE_CLOCK_OFF = 0x2;

// SBE MSG Register to know whether SBE is booted
constexpr auto SBE_MSG_REGISTER = 0x2809;
} // namespace SBE
} // namespace dump
} // namespace openpower

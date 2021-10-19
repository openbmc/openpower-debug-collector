#pragma once
namespace openpower
{
namespace dump
{
namespace SBE
{
// Dump type to the sbe_dump chipop
constexpr auto SBE_DUMP_TYPE_HOSTBOOT = 0x5;
constexpr auto SBE_DUMP_TYPE_HARDWARE = 0x1;

// SBE dump type
constexpr auto SBE_DUMP_TYPE_SBE = 0xA;

// Clock state requested
// Collect the dump with clocks on
constexpr auto SBE_CLOCK_ON = 0x1;

// Collect the dumps with clock off
constexpr auto SBE_CLOCK_OFF = 0x2;

// Dump command class
constexpr auto SBEFIFO_CMD_CLASS_DUMP = 0xAA00;

// Get dump method
constexpr auto SBEFIFO_CMD_GET_DUMP = 0x01;
} // namespace SBE
} // namespace dump
} // namespace openpower

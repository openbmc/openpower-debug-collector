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

} // namespace SBE
} // namespace dump
} // namespace openpower

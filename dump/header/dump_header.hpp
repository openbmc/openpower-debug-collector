// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openpower::dump::header
{

constexpr std::size_t bmcHeaderSize = 0x274;
constexpr std::size_t systemHeaderSize = 0x4D0;

enum class Profile
{
    bmc,
    fault,
    system,
};

enum class Generation
{
    legacy,
    next,
};

enum class Originator
{
    client,
    internal,
    supportingService,
};

struct DumpIdentifier
{
    uint32_t value{};
    std::array<char, 8> text{};
};

struct RawMetadata
{
    std::string model;
    std::string systemSerial;
    std::string bmcSerial;
    std::string hostname;
    std::optional<uint32_t> eventLogId;
    std::optional<Originator> originator;
    std::string originatorId;
};

struct NormalizedMetadata
{
    std::array<char, 8> model{};
    std::array<char, 7> systemSerial{};
    std::array<char, 12> bmcSerial{};
    std::array<char, 32> hostname{};
    std::optional<uint32_t> eventLogId;
    std::optional<Originator> originator;
    std::array<char, 32> originatorId{};
};

struct HeaderRequest
{
    Profile profile{};
    DumpIdentifier dumpId{};
    uint64_t epochSeconds{};
    uint64_t archiveSize{};
    Generation generation{};
    NormalizedMetadata metadata{};
};

Profile parseProfile(std::string_view value);
std::string_view profileName(Profile profile);
DumpIdentifier parseDumpIdentifier(Profile profile, std::string_view value);
uint32_t parseDecimalUint32(std::string_view value, std::string_view fieldName);

NormalizedMetadata normalizeMetadata(
    Profile profile, const RawMetadata& raw,
    std::vector<std::string>* warnings = nullptr);

std::vector<uint8_t> buildHeader(const HeaderRequest& request);
void validateHeader(std::span<const uint8_t> header,
                    const HeaderRequest& request);

} // namespace openpower::dump::header

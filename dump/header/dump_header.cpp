// SPDX-License-Identifier: Apache-2.0
#include "dump_header.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <ctime>
#include <limits>
#include <stdexcept>
#include <utility>

namespace openpower::dump::header
{
namespace
{

constexpr uint64_t bmcSummarySize = 0x204;
constexpr uint64_t systemSummarySize = 0x400;

class FixedWriter
{
  public:
    explicit FixedWriter(std::size_t size) : data(size, 0) {}

    void ascii(std::size_t offset, std::size_t width, std::string_view value)
    {
        require(offset, width);
        if (value.size() > width)
        {
            throw std::invalid_argument("ASCII field exceeds its fixed width");
        }
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            data[offset + index] = static_cast<uint8_t>(value[index]);
        }
    }

    template <std::size_t Size>
    void ascii(std::size_t offset, const std::array<char, Size>& value)
    {
        require(offset, Size);
        for (std::size_t index = 0; index < Size; ++index)
        {
            data[offset + index] = static_cast<uint8_t>(value[index]);
        }
    }

    void byte(std::size_t offset, uint8_t value)
    {
        require(offset, 1);
        data[offset] = value;
    }

    void be16(std::size_t offset, uint16_t value)
    {
        require(offset, sizeof(value));
        data[offset] = static_cast<uint8_t>(value >> 8);
        data[offset + 1] = static_cast<uint8_t>(value);
    }

    void be32(std::size_t offset, uint32_t value)
    {
        require(offset, sizeof(value));
        for (std::size_t index = 0; index < sizeof(value); ++index)
        {
            const auto shift = 8U * (sizeof(value) - index - 1U);
            data[offset + index] = static_cast<uint8_t>(value >> shift);
        }
    }

    void be64(std::size_t offset, uint64_t value)
    {
        require(offset, sizeof(value));
        for (std::size_t index = 0; index < sizeof(value); ++index)
        {
            const auto shift = 8U * (sizeof(value) - index - 1U);
            data[offset + index] = static_cast<uint8_t>(value >> shift);
        }
    }

    void bcdTimestamp(std::size_t offset, const std::array<char, 14>& timestamp)
    {
        require(offset, 8);
        for (std::size_t index = 0; index < timestamp.size(); index += 2)
        {
            const auto high = static_cast<uint8_t>(timestamp[index] - '0');
            const auto low = static_cast<uint8_t>(timestamp[index + 1] - '0');
            data[offset + (index / 2)] =
                static_cast<uint8_t>((high << 4) | low);
        }
    }

    std::vector<uint8_t> take() &&
    {
        return std::move(data);
    }

  private:
    void require(std::size_t offset, std::size_t width) const
    {
        if (offset > data.size() || width > data.size() - offset)
        {
            throw std::out_of_range("fixed header write exceeds the buffer");
        }
    }

    std::vector<uint8_t> data;
};

bool isPrintableAscii(std::string_view value)
{
    return std::ranges::all_of(value, [](unsigned char character) {
        return character >= 0x20 && character <= 0x7E;
    });
}

bool isAllWhitespace(std::string_view value)
{
    return std::ranges::all_of(value, [](char character) {
        return character == ' ' || character == '\t';
    });
}

bool isAsciiAlphanumeric(std::string_view value)
{
    return std::ranges::all_of(value, [](unsigned char character) {
        return (character >= '0' && character <= '9') ||
               (character >= 'A' && character <= 'Z') ||
               (character >= 'a' && character <= 'z');
    });
}

void addWarning(std::vector<std::string>* warnings, std::string message)
{
    if (warnings != nullptr)
    {
        warnings->emplace_back(std::move(message));
    }
}

std::array<char, 14> makeTimestamp(uint64_t epochSeconds)
{
    if (epochSeconds >
        static_cast<uint64_t>(std::numeric_limits<std::time_t>::max()))
    {
        throw std::out_of_range("timestamp is outside the supported range");
    }

    const auto epoch = static_cast<std::time_t>(epochSeconds);
    std::tm utc{};
    if (::gmtime_r(&epoch, &utc) == nullptr)
    {
        throw std::runtime_error("could not convert timestamp to UTC");
    }

    std::array<char, 15> formatted{};
    const auto length = std::snprintf(
        formatted.data(), formatted.size(), "%04d%02d%02d%02d%02d%02d",
        utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
        utc.tm_min, utc.tm_sec);
    if (length != 14)
    {
        throw std::runtime_error("could not format the dump timestamp");
    }

    std::array<char, 14> result{};
    std::ranges::copy_n(formatted.begin(), result.size(), result.begin());
    return result;
}

template <std::size_t Size>
std::string asString(const std::array<char, Size>& value)
{
    return {value.data(), value.size()};
}

std::string buildFileName(const HeaderRequest& request,
                          const std::array<char, 14>& timestamp)
{
    std::string prefix;
    switch (request.profile)
    {
        case Profile::bmc:
            prefix = "BMCDUMP";
            break;
        case Profile::fault:
            prefix = "FLTDUMP";
            break;
        case Profile::system:
            prefix = "SYSDUMP";
            break;
    }

    return prefix + "." + asString(request.metadata.systemSerial) + "." +
           asString(request.dumpId.text) + "." + asString(timestamp);
}

uint16_t readBe16(std::span<const uint8_t> data, std::size_t offset)
{
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1]);
}

uint32_t readBe32(std::span<const uint8_t> data, std::size_t offset)
{
    uint32_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index)
    {
        value = static_cast<uint32_t>((value << 8) | data[offset + index]);
    }
    return value;
}

uint64_t readBe64(std::span<const uint8_t> data, std::size_t offset)
{
    uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index)
    {
        value = (value << 8) | data[offset + index];
    }
    return value;
}

bool matches(std::span<const uint8_t> data, std::size_t offset,
             std::string_view expected)
{
    if (offset > data.size() || expected.size() > data.size() - offset)
    {
        return false;
    }
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        if (data[offset + index] != static_cast<uint8_t>(expected[index]))
        {
            return false;
        }
    }
    return true;
}

bool allZero(std::span<const uint8_t> data, std::size_t offset,
             std::size_t width)
{
    if (offset > data.size() || width > data.size() - offset)
    {
        return false;
    }
    return std::ranges::all_of(data.subspan(offset, width), [](uint8_t value) {
        return value == 0;
    });
}

bool matchesBcdTimestamp(std::span<const uint8_t> data, std::size_t offset,
                         const std::array<char, 14>& timestamp)
{
    if (offset > data.size() || 8 > data.size() - offset)
    {
        return false;
    }
    for (std::size_t index = 0; index < timestamp.size(); index += 2)
    {
        const auto expected = static_cast<uint8_t>(
            ((timestamp[index] - '0') << 4) | (timestamp[index + 1] - '0'));
        if (data[offset + (index / 2)] != expected)
        {
            return false;
        }
    }
    return data[offset + 7] == 0;
}

void expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        throw std::runtime_error(
            std::string("invalid generated header: ") + std::string(message));
    }
}

uint32_t contentType(uint32_t dumpId)
{
    switch (dumpId >> 24)
    {
        case 0x00:
            return 0x40000000;
        case 0x20:
            return 0x00000200;
        case 0x30:
        case 0x40:
            return 0x02000000;
        default:
            return 0;
    }
}

} // namespace

Profile parseProfile(std::string_view value)
{
    if (value == "bmc")
    {
        return Profile::bmc;
    }
    if (value == "fault")
    {
        return Profile::fault;
    }
    if (value == "system")
    {
        return Profile::system;
    }
    throw std::invalid_argument("profile must be bmc, fault, or system");
}

std::string_view profileName(Profile profile)
{
    switch (profile)
    {
        case Profile::bmc:
            return "bmc";
        case Profile::fault:
            return "fault";
        case Profile::system:
            return "system";
    }
    throw std::invalid_argument("unknown dump profile");
}

uint32_t parseDecimalUint32(std::string_view value, std::string_view fieldName)
{
    if (value.empty())
    {
        throw std::invalid_argument(
            std::string(fieldName) + " must not be empty");
    }

    uint32_t result = 0;
    const auto [end, error] =
        std::from_chars(value.data(), value.data() + value.size(), result, 10);
    if (error != std::errc{} || end != value.data() + value.size())
    {
        throw std::invalid_argument(
            std::string(fieldName) + " must be an unsigned decimal value");
    }
    return result;
}

DumpIdentifier parseDumpIdentifier(Profile profile, std::string_view value)
{
    DumpIdentifier result{};
    if (profile == Profile::system)
    {
        if (value.size() != result.text.size())
        {
            throw std::invalid_argument(
                "system dump ID must contain exactly eight hexadecimal digits");
        }
        const auto [end, error] = std::from_chars(
            value.data(), value.data() + value.size(), result.value, 16);
        if (error != std::errc{} || end != value.data() + value.size())
        {
            throw std::invalid_argument(
                "system dump ID must contain exactly eight hexadecimal digits");
        }
        std::ranges::copy(value, result.text.begin());
        return result;
    }

    if (value.size() > result.text.size())
    {
        throw std::invalid_argument(
            "BMC dump ID must contain at most eight decimal digits");
    }
    result.value = parseDecimalUint32(value, "BMC dump ID");
    if (result.value > 99999999)
    {
        throw std::out_of_range("BMC dump ID exceeds eight decimal digits");
    }

    result.text.fill('0');
    auto remaining = result.value;
    for (auto index = result.text.size(); index > 0; --index)
    {
        result.text[index - 1] = static_cast<char>('0' + (remaining % 10));
        remaining /= 10;
    }
    return result;
}

NormalizedMetadata normalizeMetadata(Profile profile, const RawMetadata& raw,
                                     std::vector<std::string>* warnings)
{
    NormalizedMetadata result{};
    result.model.fill('0');
    result.systemSerial.fill('0');
    result.bmcSerial.fill('0');

    if (raw.model.size() == result.model.size() &&
        isPrintableAscii(raw.model) && !isAllWhitespace(raw.model))
    {
        std::ranges::copy(raw.model, result.model.begin());
    }
    else
    {
        addWarning(warnings, "Model is unavailable or invalid; using 00000000");
    }

    if (!raw.systemSerial.empty() &&
        raw.systemSerial.size() <= result.systemSerial.size() &&
        isAsciiAlphanumeric(raw.systemSerial))
    {
        std::ranges::copy(raw.systemSerial,
                          result.systemSerial.end() - raw.systemSerial.size());
    }
    else
    {
        addWarning(warnings,
                   "System serial is unavailable or invalid; using 0000000");
    }

    if (profile == Profile::system)
    {
        // This field is not part of the system header.
    }
    else if (!raw.bmcSerial.empty() &&
             raw.bmcSerial.size() <= result.bmcSerial.size() &&
             isAsciiAlphanumeric(raw.bmcSerial))
    {
        std::ranges::copy(raw.bmcSerial, result.bmcSerial.begin());
    }
    else
    {
        addWarning(warnings,
                   "BMC serial is unavailable or invalid; using 000000000000");
    }

    const auto fallbackName = std::string("Server-") + asString(result.model) +
                              "-SN" + asString(result.systemSerial);
    const auto hostnameValid =
        !raw.hostname.empty() &&
        raw.hostname.size() <= result.hostname.size() &&
        isPrintableAscii(raw.hostname) && !isAllWhitespace(raw.hostname);
    const auto& hostname = profile == Profile::system && hostnameValid
                               ? raw.hostname
                               : fallbackName;
    std::ranges::copy(hostname, result.hostname.begin());
    if (profile == Profile::system && !hostnameValid)
    {
        addWarning(warnings,
                   "Hostname is unavailable or invalid; using generated name");
    }

    result.eventLogId = raw.eventLogId;
    result.originator =
        profile == Profile::system ? std::nullopt : raw.originator;
    if (profile != Profile::system &&
        raw.originatorId.size() <= result.originatorId.size() &&
        isPrintableAscii(raw.originatorId))
    {
        std::ranges::copy(raw.originatorId, result.originatorId.begin());
    }
    else if (profile != Profile::system)
    {
        addWarning(warnings,
                   "Originator ID is invalid or too long; leaving it blank");
    }
    return result;
}

static std::vector<uint8_t> buildHeaderUnchecked(const HeaderRequest& request)
{
    const auto timestamp = makeTimestamp(request.epochSeconds);
    const auto fileName = buildFileName(request, timestamp);
    expect(fileName.size() == 39, "FILE name must occupy 39 bytes");

    if (request.profile != Profile::system)
    {
        if (request.archiveSize >
            std::numeric_limits<uint64_t>::max() - bmcSummarySize)
        {
            throw std::overflow_error("BMC dump size exceeds the header field");
        }
        const auto sectionSize = request.archiveSize + bmcSummarySize;
        FixedWriter writer(bmcHeaderSize);
        writer.ascii(0x000, 8, "FILE    ");
        writer.be16(0x008, 0x0040);
        writer.be16(0x014, 0x0001);
        writer.be16(0x016, 0x000F);
        writer.ascii(0x018, 40, fileName);

        writer.ascii(0x040, 8, "SECTION ");
        writer.be16(0x048, 0x0030);
        writer.be32(0x050, 0x00000001);
        writer.be16(0x054, 0x0002);
        writer.be64(0x058, sectionSize);
        writer.ascii(0x060, 16,
                     request.profile == Profile::fault ? "FLTDUMP" : "BMCDUMP");

        writer.ascii(
            0x070, 8,
            request.profile == Profile::fault ? "FLT DUMP" : "BMC DUMP");
        writer.bcdTimestamp(0x078, timestamp);
        writer.be32(0x080, 0);
        writer.be16(0x084, 0x0210);
        writer.be16(0x086, 0x0200);
        writer.be64(0x088, sectionSize);
        writer.ascii(0x090, request.metadata.model);

        const auto systemName =
            std::string("Server-") + asString(request.metadata.model) + "-SN" +
            asString(request.metadata.systemSerial);
        writer.ascii(0x0B0, 32, systemName);
        writer.ascii(0x0D0, request.metadata.systemSerial);
        writer.be32(0x0D8, request.metadata.eventLogId.value_or(0));
        writer.be16(0x0DC, 0x0070);
        writer.ascii(0x220, request.metadata.bmcSerial);
        if (request.metadata.originator.has_value())
        {
            writer.byte(
                0x22C, static_cast<uint8_t>(
                           static_cast<uint8_t>('0') +
                           static_cast<uint8_t>(*request.metadata.originator)));
        }
        writer.ascii(0x230, request.metadata.originatorId);
        writer.byte(0x270, 0x01);
        writer.byte(0x271, 0x01);
        writer.be16(0x272, 0x0010);

        return std::move(writer).take();
    }

    if (request.archiveSize > std::numeric_limits<uint32_t>::max())
    {
        throw std::overflow_error(
            "system dump archive exceeds the 32-bit hardware section limit");
    }
    if (request.archiveSize >
        std::numeric_limits<uint64_t>::max() - systemSummarySize)
    {
        throw std::overflow_error("system dump size exceeds the header field");
    }

    FixedWriter writer(systemHeaderSize);
    writer.ascii(0x000, 8, "FILE    ");
    writer.be16(0x008, 0x0040);
    writer.be16(0x014, 0x0001);
    writer.be16(0x016, 0x000F);
    writer.ascii(0x018, 40, fileName);

    writer.ascii(0x040, 8, "SECTION ");
    writer.be16(0x048, 0x0030);
    writer.be16(0x054, 0x0002);
    writer.be64(0x058, systemSummarySize);
    writer.ascii(0x060, 16, "DUMP SUMMARY");

    writer.ascii(0x070, 8, "SECTION ");
    writer.be16(0x078, 0x0030);
    writer.be16(0x07A, 0x0002);
    writer.be16(0x084, 0x0002);
    writer.be64(0x088, request.archiveSize);
    writer.ascii(0x090, 16, "HARDWARE DATA");

    writer.ascii(0x0A0, 8, "SECTION ");
    writer.be16(0x0A8, 0x0030);
    writer.be16(0x0AA, 0x0002);
    writer.be32(0x0B0, 0x00000001);
    writer.be16(0x0B4, 0x0002);
    writer.ascii(0x0C0, 16, "HYPERVISOR DATA");

    writer.ascii(0x0D0, 8, "SYS DUMP");
    writer.bcdTimestamp(0x0D8, timestamp);
    writer.be32(0x0E0, request.dumpId.value);
    writer.be16(0x0E4,
                request.generation == Generation::legacy ? 0x0221 : 0x0222);
    writer.be16(0x0E6, 0x0400);
    writer.be64(0x0E8, request.archiveSize + systemSummarySize);
    writer.ascii(0x0F0, request.metadata.model);
    writer.ascii(0x110, request.metadata.hostname);
    writer.ascii(0x130, request.metadata.systemSerial);
    writer.byte(0x137, 0x01);
    writer.be32(0x138, request.metadata.eventLogId.value_or(0));
    writer.be16(0x13C, 0x00D0);
    writer.be32(0x330, contentType(request.dumpId.value));
    writer.byte(0x360, 0x01);

    return std::move(writer).take();
}

std::vector<uint8_t> buildHeader(const HeaderRequest& request)
{
    auto result = buildHeaderUnchecked(request);
    validateHeader(result, request);
    return result;
}

void validateHeader(std::span<const uint8_t> header,
                    const HeaderRequest& request)
{
    const auto expectedSize =
        request.profile == Profile::system ? systemHeaderSize : bmcHeaderSize;
    expect(header.size() == expectedSize, "unexpected fixed header size");
    expect(matches(header, 0x000, "FILE    "), "missing FILE eye-catcher");
    expect(readBe16(header, 0x008) == 0x0040, "incorrect FILE entry size");
    expect(allZero(header, 0x00A, 10), "nonzero FILE reserved bytes");
    expect(readBe16(header, 0x014) == 0x0001, "incorrect FILE entry type");
    expect(readBe16(header, 0x016) == 0x000F,
           "incorrect FILE name prefix length");
    expect(header[0x03F] == 0, "FILE name is not terminated");

    const auto timestamp = makeTimestamp(request.epochSeconds);
    expect(matches(header, 0x018, buildFileName(request, timestamp)),
           "incorrect FILE name");

    if (request.profile != Profile::system)
    {
        const auto sectionSize = request.archiveSize + bmcSummarySize;
        expect(matches(header, 0x040, "SECTION "),
               "missing BMC SECTION eye-catcher");
        expect(readBe16(header, 0x048) == 0x0030,
               "incorrect BMC SECTION entry size");
        expect(allZero(header, 0x04A, 6), "nonzero BMC SECTION reserved bytes");
        expect(readBe32(header, 0x050) == 1, "incorrect BMC SECTION flags");
        expect(readBe16(header, 0x054) == 2, "incorrect BMC SECTION type");
        expect(allZero(header, 0x056, 2), "nonzero BMC SECTION type padding");
        expect(readBe64(header, 0x058) == sectionSize,
               "incorrect BMC SECTION length");
        expect(
            matches(header, 0x060,
                    request.profile == Profile::fault ? "FLTDUMP" : "BMCDUMP"),
            "incorrect BMC SECTION name");
        expect(allZero(header, 0x067, 9), "nonzero BMC SECTION name padding");
        expect(matches(header, 0x070,
                       request.profile == Profile::fault ? "FLT DUMP"
                                                         : "BMC DUMP"),
               "incorrect BMC summary eye-catcher");
        expect(matchesBcdTimestamp(header, 0x078, timestamp),
               "incorrect BMC BCD timestamp");
        expect(readBe32(header, 0x080) == 0,
               "BMC summary dump ID compatibility field is nonzero");
        expect(readBe16(header, 0x084) == 0x0210,
               "incorrect BMC summary version");
        expect(readBe16(header, 0x086) == 0x0200, "incorrect BMC summary size");
        expect(readBe64(header, 0x088) == sectionSize,
               "incorrect BMC summary total size");
        expect(matches(header, 0x090, asString(request.metadata.model)) &&
                   allZero(header, 0x098, 24),
               "incorrect BMC model field");
        const auto systemName =
            std::string("Server-") + asString(request.metadata.model) + "-SN" +
            asString(request.metadata.systemSerial);
        expect(matches(header, 0x0B0, systemName) && allZero(header, 0x0C9, 7),
               "incorrect BMC system-name field");
        expect(matches(header, 0x0D0,
                       asString(request.metadata.systemSerial)) &&
                   header[0x0D7] == 0,
               "incorrect BMC system-serial field");
        expect(readBe32(header, 0x0D8) ==
                   request.metadata.eventLogId.value_or(0),
               "incorrect BMC event-log ID");
        expect(readBe16(header, 0x0DC) == 0x0070,
               "incorrect BMC directory size");
        expect(allZero(header, 0x0DE, 0x142),
               "nonzero BMC SRC reserved region");
        expect(matches(header, 0x220, asString(request.metadata.bmcSerial)),
               "incorrect BMC serial field");
        if (request.metadata.originator.has_value())
        {
            const auto expectedOriginator = static_cast<uint8_t>(
                static_cast<uint8_t>('0') +
                static_cast<uint8_t>(*request.metadata.originator));
            expect(header[0x22C] == expectedOriginator &&
                       allZero(header, 0x22D, 3),
                   "incorrect BMC originator type");
        }
        else
        {
            expect(allZero(header, 0x22C, 4),
                   "nonzero missing BMC originator type");
        }
        expect(matches(header, 0x230, asString(request.metadata.originatorId)),
               "incorrect BMC originator ID");
        expect(allZero(header, 0x250, 32), "nonzero BMC originator user ID");
        expect(header[0x270] == 1 && header[0x271] == 1 &&
                   readBe16(header, 0x272) == 0x0010,
               "incorrect BMC dump trailer");
        return;
    }

    expect(matches(header, 0x040, "SECTION ") &&
               matches(header, 0x060, "DUMP SUMMARY"),
           "incorrect system summary SECTION");
    expect(readBe16(header, 0x048) == 0x0030 && allZero(header, 0x04A, 10) &&
               readBe16(header, 0x054) == 2 && allZero(header, 0x056, 2),
           "incorrect system summary SECTION directory fields");
    expect(readBe64(header, 0x058) == systemSummarySize,
           "incorrect system summary SECTION length");
    expect(allZero(header, 0x06C, 4),
           "nonzero system summary SECTION name padding");
    expect(matches(header, 0x070, "SECTION ") &&
               matches(header, 0x090, "HARDWARE DATA"),
           "incorrect hardware SECTION");
    expect(readBe16(header, 0x078) == 0x0030 && readBe16(header, 0x07A) == 2 &&
               allZero(header, 0x07C, 8) && readBe16(header, 0x084) == 2 &&
               allZero(header, 0x086, 2),
           "incorrect hardware SECTION directory fields");
    expect(readBe64(header, 0x088) == request.archiveSize,
           "incorrect hardware SECTION length");
    expect(allZero(header, 0x09D, 3), "nonzero hardware SECTION name padding");
    expect(matches(header, 0x0A0, "SECTION ") &&
               matches(header, 0x0C0, "HYPERVISOR DATA"),
           "incorrect hypervisor SECTION");
    expect(readBe16(header, 0x0A8) == 0x0030 && readBe16(header, 0x0AA) == 2 &&
               allZero(header, 0x0AC, 4) && readBe16(header, 0x0B4) == 2 &&
               allZero(header, 0x0B6, 2),
           "incorrect hypervisor SECTION directory fields");
    expect(readBe32(header, 0x0B0) == 1,
           "hypervisor SECTION is not marked last");
    expect(readBe64(header, 0x0B8) == 0,
           "hypervisor SECTION length is not zero");
    expect(header[0x0CF] == 0, "nonzero hypervisor SECTION name padding");
    expect(matches(header, 0x0D0, "SYS DUMP"),
           "incorrect system summary eye-catcher");
    expect(matchesBcdTimestamp(header, 0x0D8, timestamp),
           "incorrect system BCD timestamp");
    expect(readBe32(header, 0x0E0) == request.dumpId.value,
           "incorrect system summary dump ID");
    expect(readBe16(header, 0x0E4) ==
               (request.generation == Generation::legacy ? 0x0221 : 0x0222),
           "incorrect system summary version");
    expect(readBe16(header, 0x0E6) == 0x0400, "incorrect system summary size");
    expect(readBe64(header, 0x0E8) == request.archiveSize + systemSummarySize,
           "incorrect system summary total size");
    expect(matches(header, 0x0F0, asString(request.metadata.model)) &&
               allZero(header, 0x0F8, 24),
           "incorrect system model field");
    expect(matches(header, 0x110, asString(request.metadata.hostname)),
           "incorrect system hostname field");
    expect(matches(header, 0x130, asString(request.metadata.systemSerial)),
           "incorrect system serial field");
    expect(header[0x137] == 1, "system summary creator is not BMC");
    expect(readBe32(header, 0x138) == request.metadata.eventLogId.value_or(0),
           "incorrect system event-log ID");
    expect(readBe16(header, 0x13C) == 0x00D0,
           "incorrect system directory size");
    expect(allZero(header, 0x13E, 0x1F2),
           "nonzero system reserved summary region");
    expect(readBe32(header, 0x330) == contentType(request.dumpId.value),
           "incorrect system content type");
    expect(allZero(header, 0x334, 44),
           "nonzero system hardware-info reserved region");
    expect(header[0x360] == 1, "system creator indicator is not BMC");
    expect(allZero(header, 0x361, 367),
           "nonzero system creator reserved region");
}

} // namespace openpower::dump::header

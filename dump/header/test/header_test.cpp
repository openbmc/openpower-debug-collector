// SPDX-License-Identifier: Apache-2.0
#include "dump_header.hpp"
#include "packager.hpp"

#include <sys/stat.h>
#ifdef __linux__
#include <sys/inotify.h>
#endif
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace openpower::dump::header;

void check(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void checkThrows(Function&& function, const std::string& message)
{
    try
    {
        function();
    }
    catch (const std::exception&)
    {
        return;
    }
    throw std::runtime_error(message);
}

uint16_t be16(std::span<const uint8_t> bytes, std::size_t offset)
{
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]);
}

uint32_t be32(std::span<const uint8_t> bytes, std::size_t offset)
{
    uint32_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index)
    {
        value = static_cast<uint32_t>((value << 8) | bytes[offset + index]);
    }
    return value;
}

uint64_t be64(std::span<const uint8_t> bytes, std::size_t offset)
{
    uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index)
    {
        value = (value << 8) | bytes[offset + index];
    }
    return value;
}

uint64_t fnv1a(std::span<const uint8_t> bytes)
{
    uint64_t hash = 14695981039346656037ULL;
    for (const auto byte : bytes)
    {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

template <std::size_t Size>
std::string asString(const std::array<char, Size>& value)
{
    return {value.data(), value.size()};
}

NormalizedMetadata sampleMetadata(Profile profile)
{
    RawMetadata raw{
        .model = "9009-42A",
        .systemSerial = "ABC1234",
        .bmcSerial = "YL10UF87E008",
        .hostname = "test-system",
        .eventLogId = 0x12345678,
        .originator = Originator::internal,
        .originatorId = "test-client",
    };
    return normalizeMetadata(profile, raw);
}

HeaderRequest sampleRequest(Profile profile, uint64_t archiveSize)
{
    return HeaderRequest{
        .profile = profile,
        .dumpId = parseDumpIdentifier(
            profile, profile == Profile::system ? "3000000E" : "296"),
        .epochSeconds = 0,
        .archiveSize = archiveSize,
        .generation = Generation::legacy,
        .metadata = sampleMetadata(profile),
    };
}

void testIdentifiers()
{
    const auto bmc = parseDumpIdentifier(Profile::bmc, "296");
    check(bmc.value == 296 && asString(bmc.text) == "00000296",
          "BMC ID was not normalized as decimal ASCII");
    const auto paddedBmc = parseDumpIdentifier(Profile::bmc, "00000296");
    check(paddedBmc.value == 296 && asString(paddedBmc.text) == "00000296",
          "zero-padded BMC ID was not parsed as decimal");
    check(asString(parseDumpIdentifier(Profile::fault, "99999999").text) ==
              "99999999",
          "maximum BMC ID was rejected");
    checkThrows([] { parseDumpIdentifier(Profile::bmc, "100000000"); },
                "overflowing BMC ID was accepted");
    checkThrows([] { parseDumpIdentifier(Profile::bmc, "-1"); },
                "negative BMC ID was accepted");
    checkThrows([] { parseDumpIdentifier(Profile::bmc, "12x"); },
                "nonnumeric BMC ID was accepted");

    const auto system = parseDumpIdentifier(Profile::system, "3000000e");
    check(system.value == 0x3000000E && asString(system.text) == "3000000e",
          "system ID text and value diverged");
    checkThrows([] { parseDumpIdentifier(Profile::system, "300000E"); },
                "short system ID was accepted");
    checkThrows([] { parseDumpIdentifier(Profile::system, "3000000Z"); },
                "invalid hexadecimal system ID was accepted");
    check(parseDecimalUint32("4294967295", "EID") ==
              std::numeric_limits<uint32_t>::max(),
          "maximum uint32 EID was rejected");
    checkThrows([] { parseDecimalUint32("4294967296", "EID"); },
                "overflowing EID was accepted");
}

void testNormalization()
{
    std::vector<std::string> warnings;
    const RawMetadata incident{
        .model = "        ",
        .systemSerial = "",
        .bmcSerial = "             ",
        .hostname = "",
        .eventLogId = std::nullopt,
        .originator = std::nullopt,
        .originatorId = std::string(33, 'x'),
    };
    const auto normalized =
        normalizeMetadata(Profile::bmc, incident, &warnings);
    check(asString(normalized.model) == "00000000",
          "whitespace model did not use the fixed fallback");
    check(asString(normalized.systemSerial) == "0000000",
          "missing system serial did not use the fixed fallback");
    check(asString(normalized.bmcSerial) == "000000000000",
          "invalid BMC serial did not use the fixed fallback");
    check(asString(normalized.originatorId) == std::string(32, '\0'),
          "overlong originator ID was not cleared");
    check(!warnings.empty(), "metadata fallbacks were not reported");

    RawMetadata special{
        .model = "AB%\\1234",
        .systemSerial = "42",
        .bmcSerial = "ABC123",
        .hostname = std::string(32, 'h'),
        .eventLogId = std::nullopt,
        .originator = Originator::client,
        .originatorId = std::string(32, '%'),
    };
    const auto exact = normalizeMetadata(Profile::bmc, special);
    check(asString(exact.model) == special.model,
          "safe punctuation was modified");
    check(asString(exact.systemSerial) == "0000042",
          "short serial was not left padded");
    check(asString(exact.bmcSerial) == "ABC123000000",
          "short BMC serial was not right padded");
    check(asString(exact.originatorId) == special.originatorId,
          "32-byte originator ID was not preserved");

    special.model = "123456789";
    special.bmcSerial = "1234567890123";
    special.hostname = std::string(33, 'h');
    const auto overlong = normalizeMetadata(Profile::system, special);
    check(asString(overlong.model) == "00000000",
          "overlong model was not rejected");
    check(asString(overlong.hostname).starts_with("Server-00000000-SN"),
          "overlong hostname did not use the generated fallback");

    special.model = "123456\xC3\xA9";
    const auto nonAscii = normalizeMetadata(Profile::bmc, special);
    check(asString(nonAscii.model) == "00000000",
          "multibyte model was not rejected");
}

void testBmcHeaders()
{
    auto request = sampleRequest(Profile::bmc, 4);
    auto header = buildHeader(request);
    check(header.size() == bmcHeaderSize,
          "BMC header does not end at payload offset 0x274");
    check(be16(header, 0x008) == 0x40 && be16(header, 0x048) == 0x30,
          "BMC directory lengths are incorrect");
    check(be64(header, 0x058) == 4 + 0x204 && be64(header, 0x088) == 4 + 0x204,
          "BMC section sizes are incorrect");
    check(be32(header, 0x080) == 0, "BMC compatibility dump-ID field changed");
    check(be32(header, 0x0D8) == 0x12345678, "BMC event-log ID is incorrect");
    check(header[0x078] == 0x19 && header[0x079] == 0x70 &&
              header[0x07A] == 0x01 && header[0x07B] == 0x01,
          "BMC UTC BCD timestamp is incorrect");
    check(header[0x22C] == '1' && header[0x22D] == 0,
          "legacy ASCII originator type changed");
    check(header[0x270] == 1 && header[0x271] == 1 &&
              be16(header, 0x272) == 0x10,
          "BMC trailer is incorrect");

    // Golden hash of the corrected header from the eight-space Model incident.
    const RawMetadata incident{
        .model = "        ",
        .systemSerial = "",
        .bmcSerial = "YF33UF19Y00J",
        .hostname = "",
        .eventLogId = std::nullopt,
        .originator = std::nullopt,
        .originatorId = "",
    };
    request = HeaderRequest{
        .profile = Profile::bmc,
        .dumpId = parseDumpIdentifier(Profile::bmc, "296"),
        .epochSeconds = 1787052378,
        .archiveSize = 2124772,
        .generation = Generation::legacy,
        .metadata = normalizeMetadata(Profile::bmc, incident),
    };
    check(fnv1a(buildHeader(request)) == 0xDF2B55B507FAC216ULL,
          "incident regression header differs from the corrected golden bytes");

    request.profile = Profile::fault;
    request.dumpId = parseDumpIdentifier(Profile::fault, "296");
    header = buildHeader(request);
    check(std::string(header.begin() + 0x060, header.begin() + 0x067) ==
                  "FLTDUMP" &&
              std::string(header.begin() + 0x070, header.begin() + 0x078) ==
                  "FLT DUMP",
          "fault header tokens changed");

    request = sampleRequest(Profile::bmc,
                            std::numeric_limits<uint64_t>::max() - 0x204);
    check(buildHeader(request).size() == bmcHeaderSize,
          "maximum BMC section size was rejected");
    request.archiveSize++;
    checkThrows([&request] { buildHeader(request); },
                "overflowing BMC section size was accepted");

    request = sampleRequest(Profile::bmc, 1);
    header = buildHeader(request);
    header[0x084] = 0;
    checkThrows([&header, &request] { validateHeader(header, request); },
                "corrupt BMC header passed validation");
    header = buildHeader(request);
    header[0x100] = 1;
    checkThrows([&header, &request] { validateHeader(header, request); },
                "nonzero reserved BMC byte passed validation");
    header = buildHeader(request);
    header[0x053] = 2;
    checkThrows([&header, &request] { validateHeader(header, request); },
                "corrupt BMC SECTION flags passed validation");
    header = buildHeader(request);
    checkThrows(
        [&header, &request] {
            validateHeader(
                std::span<const uint8_t>(header.data(), header.size() - 1),
                request);
        },
        "truncated BMC header passed validation");
}

void testSystemHeaders()
{
    auto request = sampleRequest(Profile::system, 8);
    request.generation = Generation::legacy;
    auto header = buildHeader(request);
    check(header.size() == systemHeaderSize,
          "system header does not end at payload offset 0x4D0");
    check(be64(header, 0x058) == 0x400 && be64(header, 0x088) == 8,
          "system SECTION lengths are incorrect");
    check(be32(header, 0x0E0) == 0x3000000E,
          "system BE32 dump ID is incorrect");
    check(be16(header, 0x0E4) == 0x0221,
          "legacy system header version is incorrect");
    check(be64(header, 0x0E8) == 8 + 0x400, "system total size is incorrect");
    check(be32(header, 0x330) == 0x02000000,
          "system content type is incorrect");
    check(header[0x360] == 1, "system BMC creator byte is incorrect");

    request.generation = Generation::next;
    header = buildHeader(request);
    check(be16(header, 0x0E4) == 0x0222,
          "next-generation system header version is incorrect");

    const RawMetadata goldenMetadata{
        .model = "9105-42A",
        .systemSerial = "SIMP10R",
        .bmcSerial = "",
        .hostname = "huygens",
        .eventLogId = std::nullopt,
        .originator = std::nullopt,
        .originatorId = "",
    };
    const HeaderRequest goldenRequest{
        .profile = Profile::system,
        .dumpId = parseDumpIdentifier(Profile::system, "20000001"),
        .epochSeconds = 1783077335,
        .archiveSize = 398411,
        .generation = Generation::next,
        .metadata = normalizeMetadata(Profile::system, goldenMetadata),
    };
    check(fnv1a(buildHeader(goldenRequest)) == 0x055CB61042F18BA9ULL,
          "system header differs from the known-good sample bytes");

    request.archiveSize = std::numeric_limits<uint32_t>::max();
    check(buildHeader(request).size() == systemHeaderSize,
          "maximum hardware SECTION length was rejected");
    request.archiveSize++;
    checkThrows([&request] { buildHeader(request); },
                "overflowing hardware SECTION length was accepted");

    request = sampleRequest(Profile::system, 8);
    header = buildHeader(request);
    header[0x08F] = 9;
    checkThrows([&header, &request] { validateHeader(header, request); },
                "corrupt system SECTION length passed validation");
}

class TemporaryDirectory
{
  public:
    TemporaryDirectory()
    {
        std::array<char, 64> pattern{};
        const std::string prefix = "/tmp/opdc-header-test.XXXXXX";
        std::copy(prefix.begin(), prefix.end(), pattern.begin());
        const auto created = ::mkdtemp(pattern.data());
        if (created == nullptr)
        {
            throw std::runtime_error("could not create test directory");
        }
        path = created;
    }
    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path;
};

void writeFile(const std::filesystem::path& path,
               std::span<const uint8_t> bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output)
    {
        throw std::runtime_error("could not write test archive");
    }
}

std::vector<uint8_t> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

std::vector<uint8_t> payload(Profile profile)
{
    if (profile == Profile::bmc)
    {
        return {0x28, 0xB5, 0x2F, 0xFD, 0x10, 0x20};
    }
    if (profile == Profile::system)
    {
        return {0x1F, 0x8B, 0x08, 0x00, 0x10, 0x20};
    }
    std::vector<uint8_t> result(512, 0);
    result[257] = 'u';
    result[258] = 's';
    result[259] = 't';
    result[260] = 'a';
    result[261] = 'r';
    return result;
}

void testPackaging()
{
    TemporaryDirectory directory;
    for (const auto profile : {Profile::bmc, Profile::fault, Profile::system})
    {
        const auto archive = directory.path / std::string(profileName(profile));
        const auto original = payload(profile);
        writeFile(archive, original);
        check(::chmod(archive.c_str(), 0644) == 0,
              "could not set test archive mode");

        const PackageRequest request{
            .profile = profile,
            .archive = archive,
            .output = archive,
            .maximumArchiveSize = std::nullopt,
            .dumpId = parseDumpIdentifier(
                profile, profile == Profile::system ? "3000000E" : "296"),
            .epochSeconds = 0,
            .generation = Generation::legacy,
            .metadata = sampleMetadata(profile),
        };
        packageDump(request);
        const auto packaged = readFile(archive);
        const auto offset =
            profile == Profile::system ? systemHeaderSize : bmcHeaderSize;
        check(packaged.size() == offset + original.size(),
              "packaged dump size is incorrect");
        const HeaderRequest headerRequest{
            .profile = request.profile,
            .dumpId = request.dumpId,
            .epochSeconds = request.epochSeconds,
            .archiveSize = original.size(),
            .generation = request.generation,
            .metadata = request.metadata,
        };
        validateHeader(std::span<const uint8_t>(packaged.data(), offset),
                       headerRequest);
        check(
            std::equal(original.begin(), original.end(),
                       packaged.begin() + static_cast<std::ptrdiff_t>(offset)),
            "packager changed payload bytes");
        struct stat status{};
        check(::stat(archive.c_str(), &status) == 0 &&
                  (status.st_mode & 0777) == 0644,
              "packager did not preserve archive permissions");
    }

    const auto invalid = directory.path / "invalid";
    const std::vector<uint8_t> badPayload = {1, 2, 3, 4};
    writeFile(invalid, badPayload);
    auto request = PackageRequest{
        .profile = Profile::bmc,
        .archive = invalid,
        .output = invalid,
        .maximumArchiveSize = std::nullopt,
        .dumpId = parseDumpIdentifier(Profile::bmc, "296"),
        .epochSeconds = 0,
        .generation = Generation::legacy,
        .metadata = sampleMetadata(Profile::bmc),
    };
    checkThrows([&request] { packageDump(request); },
                "invalid payload signature was accepted");
    check(readFile(invalid) == badPayload,
          "failed packaging changed the source archive");

    const auto limited = directory.path / "limited";
    const auto validPayload = payload(Profile::bmc);
    writeFile(limited, validPayload);
    request.archive = limited;
    request.output = limited;
    request.maximumArchiveSize = validPayload.size() - 1;
    checkThrows([&request] { packageDump(request); },
                "archive size limit was not enforced");
    check(readFile(limited) == validPayload,
          "size rejection changed the source archive");

    const auto exact = directory.path / "exact-limit";
    writeFile(exact, validPayload);
    request.archive = exact;
    request.output = exact;
    request.maximumArchiveSize = validPayload.size();
    packageDump(request);
    check(readFile(exact).size() == bmcHeaderSize + validPayload.size(),
          "archive at the exact size limit was rejected");

    const auto systemSource = directory.path / "system-source";
    const auto systemPayload = payload(Profile::system);
    const auto publishDirectory = directory.path / "published";
    std::filesystem::create_directory(publishDirectory);
    const auto systemOutput = publishDirectory / "SYSDUMP.output";
    writeFile(systemSource, systemPayload);
    const PackageRequest systemRequest{
        .profile = Profile::system,
        .archive = systemSource,
        .output = systemOutput,
        .maximumArchiveSize = std::nullopt,
        .dumpId = parseDumpIdentifier(Profile::system, "3000000E"),
        .epochSeconds = 0,
        .generation = Generation::legacy,
        .metadata = sampleMetadata(Profile::system),
    };
#ifdef __linux__
    const auto inotifyDescriptor = ::inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    check(inotifyDescriptor >= 0, "could not create publication watcher");
    const auto watch =
        ::inotify_add_watch(inotifyDescriptor, publishDirectory.c_str(),
                            IN_CREATE | IN_CLOSE_WRITE | IN_MOVED_TO);
    check(watch >= 0, "could not watch publication directory");
#endif
    packageDump(systemRequest);
#ifdef __linux__
    std::array<char, 4096> eventBuffer{};
    const auto eventBytes =
        ::read(inotifyDescriptor, eventBuffer.data(), eventBuffer.size());
    check(eventBytes > 0, "packager emitted no publication events");
    bool finalClose = false;
    bool finalMove = false;
    bool temporaryClose = false;
    std::size_t eventOffset = 0;
    while (eventOffset < static_cast<std::size_t>(eventBytes))
    {
        const auto* event = reinterpret_cast<const inotify_event*>(
            eventBuffer.data() + eventOffset);
        const std::string eventName = event->len == 0 ? "" : event->name;
        if ((event->mask & IN_CLOSE_WRITE) != 0)
        {
            finalClose = finalClose ||
                         eventName == systemOutput.filename().string();
            temporaryClose = temporaryClose ||
                             eventName.find(".tmp.") != std::string::npos;
        }
        if ((event->mask & IN_MOVED_TO) != 0 &&
            eventName == systemOutput.filename().string())
        {
            finalMove = true;
        }
        eventOffset += sizeof(inotify_event) + event->len;
    }
    check(::close(inotifyDescriptor) == 0,
          "could not close publication watcher");
    check(finalMove && finalClose && !temporaryClose,
          "atomic publish did not close only the completed final name");
#endif
    check(readFile(systemSource) == systemPayload,
          "distinct-output packaging changed the source archive");
    const auto published = readFile(systemOutput);
    check(published.size() == systemHeaderSize + systemPayload.size() &&
              std::equal(systemPayload.begin(), systemPayload.end(),
                         published.begin() + systemHeaderSize),
          "distinct-output packaging produced invalid payload placement");

    const auto existing = directory.path / "existing";
    const std::vector<uint8_t> existingData = {9, 8, 7};
    writeFile(existing, existingData);
    request.archive = limited;
    request.maximumArchiveSize = std::nullopt;
    request.output = existing;
#ifdef __linux__
    const auto failureInotify = ::inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    check(failureInotify >= 0, "could not create failure watcher");
    const auto failureWatch = ::inotify_add_watch(
        failureInotify, directory.path.c_str(),
        IN_CREATE | IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE);
    check(failureWatch >= 0, "could not watch failed publication");
#endif
    checkThrows([&request] { packageDump(request); },
                "packager overwrote an existing distinct output");
#ifdef __linux__
    std::array<char, 4096> failureEvents{};
    const auto failureEventBytes =
        ::read(failureInotify, failureEvents.data(), failureEvents.size());
    check(failureEventBytes > 0,
          "failed packaging emitted no staging-directory events");
    bool parentClose = false;
    std::size_t failureEventOffset = 0;
    while (failureEventOffset < static_cast<std::size_t>(failureEventBytes))
    {
        const auto* event = reinterpret_cast<const inotify_event*>(
            failureEvents.data() + failureEventOffset);
        parentClose = parentClose || (event->mask & IN_CLOSE_WRITE) != 0;
        failureEventOffset += sizeof(inotify_event) + event->len;
    }
    check(::close(failureInotify) == 0, "could not close failure watcher");
    check(!parentClose,
          "failed staging write emitted a parent-directory close event");
#endif
    check(readFile(existing) == existingData,
          "no-clobber failure changed the existing output");

    const auto concurrentSource = directory.path / "concurrent-source";
    const auto concurrentOutput = directory.path / "concurrent-output";
    writeFile(concurrentSource, validPayload);
    const PackageRequest concurrentRequest{
        .profile = Profile::bmc,
        .archive = concurrentSource,
        .output = concurrentOutput,
        .maximumArchiveSize = std::nullopt,
        .dumpId = parseDumpIdentifier(Profile::bmc, "296"),
        .epochSeconds = 0,
        .generation = Generation::legacy,
        .metadata = sampleMetadata(Profile::bmc),
    };
    const auto spawnPackager = [&concurrentRequest] {
        const auto child = ::fork();
        check(child >= 0, "could not fork concurrent packager");
        if (child == 0)
        {
            try
            {
                packageDump(concurrentRequest);
                ::_exit(0);
            }
            catch (const std::exception&)
            {
                ::_exit(1);
            }
        }
        return child;
    };
    const auto firstChild = spawnPackager();
    const auto secondChild = spawnPackager();
    int firstStatus = 0;
    int secondStatus = 0;
    check(::waitpid(firstChild, &firstStatus, 0) == firstChild &&
              ::waitpid(secondChild, &secondStatus, 0) == secondChild,
          "could not collect concurrent packager status");
    check(
        WIFEXITED(firstStatus) && WIFEXITED(secondStatus) &&
            ((WEXITSTATUS(firstStatus) == 0 &&
              WEXITSTATUS(secondStatus) == 1) ||
             (WEXITSTATUS(firstStatus) == 1 && WEXITSTATUS(secondStatus) == 0)),
        "concurrent packagers did not publish exactly one output");
    const auto concurrentPackage = readFile(concurrentOutput);
    check(concurrentPackage.size() == bmcHeaderSize + validPayload.size() &&
              std::equal(validPayload.begin(), validPayload.end(),
                         concurrentPackage.begin() + bmcHeaderSize),
          "concurrent publication produced an invalid package");

    request.archive = directory.path / "missing";
    request.output = request.archive;
    checkThrows([&request] { packageDump(request); },
                "missing archive was accepted");

    const auto symlink = directory.path / "archive-link";
    std::filesystem::create_symlink(limited, symlink);
    request.archive = symlink;
    request.output = symlink;
    checkThrows([&request] { packageDump(request); },
                "symlink archive was accepted");

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(directory.path))
    {
        check(entry.path().filename().string().find(".tmp.") ==
                  std::string::npos,
              "failed packaging left a temporary file");
    }
}

} // namespace

int main()
{
    try
    {
        testIdentifiers();
        testNormalization();
        testBmcHeaders();
        testSystemHeaders();
        testPackaging();
    }
    catch (const std::exception& error)
    {
        std::cerr << "header_test: " << error.what() << '\n';
        return 1;
    }
    std::cout << "header_test: all checks passed\n";
    return 0;
}

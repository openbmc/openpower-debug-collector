// SPDX-License-Identifier: Apache-2.0
#include "config.h"

#include "dump_header.hpp"
#include "metadata.hpp"
#include "packager.hpp"

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

uint64_t parseDecimalUint64(std::string_view value, std::string_view field)
{
    uint64_t result = 0;
    const auto [end, error] =
        std::from_chars(value.data(), value.data() + value.size(), result, 10);
    if (value.empty() || error != std::errc{} ||
        end != value.data() + value.size())
    {
        throw std::invalid_argument(
            std::string(field) + " must be an unsigned decimal value");
    }
    return result;
}

openpower::dump::header::Generation configuredGeneration()
{
    constexpr std::string_view backend = PHAL_BACKEND;
    if (backend == "legacy")
    {
        return openpower::dump::header::Generation::legacy;
    }
    if (backend == "next")
    {
        return openpower::dump::header::Generation::next;
    }
    throw std::runtime_error(
        "unsupported PHAL backend: " + std::string(backend));
}

bool knownSystemContent(uint32_t dumpId)
{
    switch (dumpId >> 24)
    {
        case 0x00:
        case 0x20:
        case 0x30:
        case 0x40:
            return true;
        default:
            return false;
    }
}

} // namespace

int main(int argc, char** argv)
{
    using namespace openpower::dump::header;

    CLI::App app{"Generate and atomically apply a fixed IBM dump header",
                 "op-dump-packager"};
    std::string profileText;
    std::string archiveText;
    std::string outputText;
    std::string dumpIdText;
    std::string epochText;
    std::optional<std::string> eventLogIdText;
    std::optional<std::string> pelPath;
    std::optional<std::string> maximumSizeText;

    app.add_option("--profile", profileText, "bmc, fault, or system")
        ->required();
    app.add_option("--archive", archiveText, "Unheadered dump archive")
        ->required();
    app.add_option("--output", outputText,
                   "Packaged output (defaults to replacing the archive)");
    app.add_option("--dump-id", dumpIdText,
                   "Decimal BMC ID or eight-digit hexadecimal system ID")
        ->required();
    app.add_option("--epoch", epochText, "UTC timestamp in epoch seconds")
        ->required();
    app.add_option("--event-log-id", eventLogIdText,
                   "Decimal uint32 system error-log ID");
    app.add_option("--pel-path", pelPath,
                   "PEL entry path used to obtain the BMC PlatformLogID");
    app.add_option("--max-size", maximumSizeText,
                   "Maximum unheadered archive size in bytes");

    try
    {
        CLI11_PARSE(app, argc, argv);
    }
    catch (const CLI::ParseError& error)
    {
        return app.exit(error);
    }

    try
    {
        const auto profile = parseProfile(profileText);
        if (eventLogIdText.has_value() && profile != Profile::system)
        {
            throw std::invalid_argument(
                "--event-log-id is valid only for the system profile");
        }
        if (pelPath.has_value() && profile == Profile::system)
        {
            throw std::invalid_argument(
                "--pel-path is valid only for BMC and fault profiles");
        }

        const auto dumpId = parseDumpIdentifier(profile, dumpIdText);
        const auto epoch = parseDecimalUint64(epochText, "epoch");
        const auto maximumSize =
            maximumSizeText.has_value()
                ? std::optional<uint64_t>(
                      parseDecimalUint64(*maximumSizeText, "maximum size"))
                : std::nullopt;
        const auto eventLogId =
            eventLogIdText.has_value()
                ? std::optional<uint32_t>(
                      parseDecimalUint32(*eventLogIdText, "event-log ID"))
                : std::nullopt;
        const auto generation = profile == Profile::system
                                    ? configuredGeneration()
                                    : Generation::legacy;

        auto rawMetadata = readMetadata(profile, dumpId.value, pelPath);
        if (eventLogId.has_value())
        {
            rawMetadata.eventLogId = eventLogId;
        }

        std::vector<std::string> warnings;
        const auto metadata =
            normalizeMetadata(profile, rawMetadata, &warnings);
        for (const auto& warning : warnings)
        {
            std::cerr << "op-dump-packager: " << warning << '\n';
        }
        if (profile == Profile::system && !knownSystemContent(dumpId.value))
        {
            std::cerr
                << "op-dump-packager: unknown system dump content prefix; "
                   "using compatibility value 0\n";
        }

        const std::filesystem::path archive{archiveText};
        const std::filesystem::path output =
            outputText.empty() ? archive : std::filesystem::path(outputText);
        const PackageRequest request{
            .profile = profile,
            .archive = archive,
            .output = output,
            .maximumArchiveSize = maximumSize,
            .dumpId = dumpId,
            .epochSeconds = epoch,
            .generation = generation,
            .metadata = metadata,
        };
        packageDump(request);
    }
    catch (const ArchiveTooLarge& error)
    {
        std::cerr << "op-dump-packager: " << error.what() << '\n';
        return 2;
    }
    catch (const std::exception& error)
    {
        std::cerr << "op-dump-packager: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

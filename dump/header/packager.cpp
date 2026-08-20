// SPDX-License-Identifier: Apache-2.0
#include "packager.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace openpower::dump::header
{
namespace
{

class FileDescriptor
{
  public:
    explicit FileDescriptor(int value = -1) : value(value) {}
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(FileDescriptor&& other) noexcept :
        value(std::exchange(other.value, -1))
    {}
    FileDescriptor& operator=(FileDescriptor&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            value = std::exchange(other.value, -1);
        }
        return *this;
    }
    ~FileDescriptor()
    {
        reset();
    }

    int get() const
    {
        return value;
    }

    void reset() noexcept
    {
        if (value >= 0)
        {
            const auto unused = ::close(value);
            static_cast<void>(unused);
            value = -1;
        }
    }

  private:
    int value;
};

class TemporaryDirectory
{
  public:
    explicit TemporaryDirectory(std::filesystem::path value) :
        value(std::move(value))
    {}
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    ~TemporaryDirectory()
    {
        const auto unused = ::rmdir(value.c_str());
        static_cast<void>(unused);
    }

  private:
    std::filesystem::path value;
};

class TemporaryPath
{
  public:
    explicit TemporaryPath(std::filesystem::path value) :
        value(std::move(value))
    {}
    TemporaryPath(const TemporaryPath&) = delete;
    TemporaryPath& operator=(const TemporaryPath&) = delete;
    ~TemporaryPath()
    {
        if (active)
        {
            const auto unused = ::unlink(value.c_str());
            static_cast<void>(unused);
        }
    }

    const std::filesystem::path& get() const
    {
        return value;
    }

    void release()
    {
        active = false;
    }

  private:
    std::filesystem::path value;
    bool active = true;
};

[[noreturn]] void throwSystemError(std::string_view action,
                                   const std::filesystem::path& path)
{
    throw std::system_error(errno, std::generic_category(),
                            std::string(action) + ": " + path.string());
}

void writeAll(int descriptor, std::span<const uint8_t> bytes)
{
    std::size_t offset = 0;
    while (offset < bytes.size())
    {
        const auto count =
            ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
        if (count < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            throw std::system_error(errno, std::generic_category(),
                                    "could not write packaged dump");
        }
        if (count == 0)
        {
            throw std::runtime_error("short write while packaging dump");
        }
        offset += static_cast<std::size_t>(count);
    }
}

std::array<uint8_t, 262> readSignature(int descriptor)
{
    std::array<uint8_t, 262> result{};
    std::size_t offset = 0;
    while (offset < result.size())
    {
        const auto count =
            ::pread(descriptor, result.data() + offset, result.size() - offset,
                    static_cast<off_t>(offset));
        if (count < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            throw std::system_error(errno, std::generic_category(),
                                    "could not inspect dump archive");
        }
        if (count == 0)
        {
            break;
        }
        offset += static_cast<std::size_t>(count);
    }
    return result;
}

void validateArchive(Profile profile, int descriptor, uint64_t archiveSize)
{
    const auto signature = readSignature(descriptor);
    switch (profile)
    {
        case Profile::bmc:
            if (archiveSize < 4 || signature[0] != 0x28 ||
                signature[1] != 0xB5 || signature[2] != 0x2F ||
                signature[3] != 0xFD)
            {
                throw std::invalid_argument(
                    "BMC dump payload is not a Zstandard archive");
            }
            break;
        case Profile::fault:
            if (archiveSize < signature.size() || signature[257] != 'u' ||
                signature[258] != 's' || signature[259] != 't' ||
                signature[260] != 'a' || signature[261] != 'r')
            {
                throw std::invalid_argument(
                    "fault dump payload is not an uncompressed tar archive");
            }
            break;
        case Profile::system:
            if (archiveSize < 2 || signature[0] != 0x1F || signature[1] != 0x8B)
            {
                throw std::invalid_argument(
                    "system dump payload is not a gzip archive");
            }
            break;
    }
}

FileDescriptor createTemporary(const std::filesystem::path& output,
                               std::filesystem::path& stagingDirectory,
                               std::filesystem::path& createdPath)
{
    auto directory = output.parent_path();
    if (directory.empty())
    {
        directory = ".";
    }
    auto stagingPattern = (directory / ".op-dump-packager.XXXXXX").string();
    std::vector<char> mutableStagingPattern(stagingPattern.begin(),
                                            stagingPattern.end());
    mutableStagingPattern.push_back('\0');
    const auto createdDirectory = ::mkdtemp(mutableStagingPattern.data());
    if (createdDirectory == nullptr)
    {
        throwSystemError("could not create package staging directory", output);
    }
    stagingDirectory = createdDirectory;

    auto pattern = (stagingDirectory / "package.XXXXXX").string();
    std::vector<char> mutablePattern(pattern.begin(), pattern.end());
    mutablePattern.push_back('\0');
    const auto descriptor = ::mkstemp(mutablePattern.data());
    if (descriptor < 0)
    {
        const auto savedError = errno;
        const auto unusedRemove = ::rmdir(stagingDirectory.c_str());
        static_cast<void>(unusedRemove);
        errno = savedError;
        throwSystemError("could not create temporary package", output);
    }
    createdPath = mutablePattern.data();
    if (::fcntl(descriptor, F_SETFD, FD_CLOEXEC) != 0)
    {
        const auto savedError = errno;
        const auto unusedClose = ::close(descriptor);
        const auto unusedUnlink = ::unlink(createdPath.c_str());
        const auto unusedRemove = ::rmdir(stagingDirectory.c_str());
        static_cast<void>(unusedClose);
        static_cast<void>(unusedUnlink);
        static_cast<void>(unusedRemove);
        errno = savedError;
        throwSystemError("could not secure temporary package", createdPath);
    }
    return FileDescriptor(descriptor);
}

bool sameFile(const struct stat& left, const struct stat& right)
{
    const auto common = left.st_dev == right.st_dev &&
                        left.st_ino == right.st_ino &&
                        left.st_size == right.st_size;
#ifdef __APPLE__
    return common && left.st_mtimespec.tv_sec == right.st_mtimespec.tv_sec &&
           left.st_mtimespec.tv_nsec == right.st_mtimespec.tv_nsec &&
           left.st_ctimespec.tv_sec == right.st_ctimespec.tv_sec &&
           left.st_ctimespec.tv_nsec == right.st_ctimespec.tv_nsec;
#else
    return common && left.st_mtim.tv_sec == right.st_mtim.tv_sec &&
           left.st_mtim.tv_nsec == right.st_mtim.tv_nsec &&
           left.st_ctim.tv_sec == right.st_ctim.tv_sec &&
           left.st_ctim.tv_nsec == right.st_ctim.tv_nsec;
#endif
}

void lockSource(int descriptor)
{
    while (::flock(descriptor, LOCK_EX) != 0)
    {
        if (errno != EINTR)
        {
            throw std::system_error(errno, std::generic_category(),
                                    "could not lock dump archive");
        }
    }
}

FileDescriptor openOutputDirectory(const std::filesystem::path& output)
{
    auto directory = output.parent_path();
    if (directory.empty())
    {
        directory = ".";
    }
    auto flags = O_RDONLY;
#ifdef O_DIRECTORY
    flags |= O_DIRECTORY;
#endif
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
    FileDescriptor descriptor(::open(directory.c_str(), flags));
    if (descriptor.get() < 0)
    {
        throwSystemError("could not open output directory", directory);
    }
    return descriptor;
}

void publishNoReplace(const std::filesystem::path& temporary,
                      const std::filesystem::path& output)
{
#ifdef __linux__
    constexpr unsigned int renameNoReplace = 1;
    if (::syscall(SYS_renameat2, AT_FDCWD, temporary.c_str(), AT_FDCWD,
                  output.c_str(), renameNoReplace) != 0)
    {
        throwSystemError("could not publish packaged dump", output);
    }
#else
    struct stat existing{};
    if (::lstat(output.c_str(), &existing) == 0)
    {
        throw std::filesystem::filesystem_error(
            "output already exists", output,
            std::make_error_code(std::errc::file_exists));
    }
    if (errno != ENOENT || ::rename(temporary.c_str(), output.c_str()) != 0)
    {
        throwSystemError("could not publish packaged dump", output);
    }
#endif
}

} // namespace

void packageDump(const PackageRequest& request)
{
    if (request.archive.empty() || request.output.empty() ||
        request.output.filename().empty())
    {
        throw std::invalid_argument("archive and output paths are required");
    }

    auto openFlags = O_RDONLY;
#ifdef O_CLOEXEC
    openFlags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    openFlags |= O_NOFOLLOW;
#endif
    FileDescriptor source(::open(request.archive.c_str(), openFlags));
    if (source.get() < 0)
    {
        throwSystemError("could not open dump archive", request.archive);
    }
    lockSource(source.get());

    struct stat originalStatus{};
    if (::fstat(source.get(), &originalStatus) != 0)
    {
        throwSystemError("could not inspect dump archive", request.archive);
    }
    if (!S_ISREG(originalStatus.st_mode) || originalStatus.st_size < 0)
    {
        throw std::invalid_argument("dump archive must be a regular file");
    }

    struct stat initialOutputStatus{};
    const auto outputExists =
        ::lstat(request.output.c_str(), &initialOutputStatus) == 0;
    if (!outputExists && errno != ENOENT)
    {
        throwSystemError("could not inspect package output", request.output);
    }
    const auto replacesSource =
        outputExists && initialOutputStatus.st_dev == originalStatus.st_dev &&
        initialOutputStatus.st_ino == originalStatus.st_ino;

    const auto archiveSize = static_cast<uint64_t>(originalStatus.st_size);
    if (request.maximumArchiveSize.has_value() &&
        archiveSize > *request.maximumArchiveSize)
    {
        throw ArchiveTooLarge("dump archive exceeds the configured size limit");
    }
    validateArchive(request.profile, source.get(), archiveSize);

    const HeaderRequest headerRequest{
        .profile = request.profile,
        .dumpId = request.dumpId,
        .epochSeconds = request.epochSeconds,
        .archiveSize = archiveSize,
        .generation = request.generation,
        .metadata = request.metadata,
    };
    const auto header = buildHeader(headerRequest);

    std::filesystem::path stagingDirectory;
    std::filesystem::path temporaryName;
    auto temporary =
        createTemporary(request.output, stagingDirectory, temporaryName);
    TemporaryDirectory stagingCleanup(stagingDirectory);
    TemporaryPath cleanup(temporaryName);

    writeAll(temporary.get(), header);
    std::array<uint8_t, 128 * 1024> buffer{};
    uint64_t copied = 0;
    while (true)
    {
        const auto count = ::read(source.get(), buffer.data(), buffer.size());
        if (count < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            throwSystemError("could not read dump archive", request.archive);
        }
        if (count == 0)
        {
            break;
        }
        const auto bytes = static_cast<std::size_t>(count);
        if (copied > archiveSize || bytes > archiveSize - copied)
        {
            throw std::runtime_error(
                "dump archive changed while it was being packaged");
        }
        writeAll(temporary.get(),
                 std::span<const uint8_t>(buffer.data(), bytes));
        copied += bytes;
    }
    if (copied != archiveSize)
    {
        throw std::runtime_error(
            "dump archive changed while it was being packaged");
    }

    struct stat finalSourceStatus{};
    struct stat pathStatus{};
    if (::fstat(source.get(), &finalSourceStatus) != 0 ||
        ::lstat(request.archive.c_str(), &pathStatus) != 0 ||
        !sameFile(originalStatus, finalSourceStatus) ||
        finalSourceStatus.st_dev != pathStatus.st_dev ||
        finalSourceStatus.st_ino != pathStatus.st_ino)
    {
        throw std::runtime_error(
            "dump archive changed while it was being packaged");
    }
    if (replacesSource)
    {
        struct stat finalOutputStatus{};
        if (::lstat(request.output.c_str(), &finalOutputStatus) != 0 ||
            finalOutputStatus.st_dev != finalSourceStatus.st_dev ||
            finalOutputStatus.st_ino != finalSourceStatus.st_ino)
        {
            throw std::runtime_error(
                "dump package output changed while it was being built");
        }
    }

    struct stat packagedStatus{};
    if (::fstat(temporary.get(), &packagedStatus) != 0)
    {
        throwSystemError("could not inspect packaged dump", temporaryName);
    }
    const auto expectedSize = archiveSize + header.size();
    if (packagedStatus.st_size < 0 ||
        static_cast<uint64_t>(packagedStatus.st_size) != expectedSize)
    {
        throw std::runtime_error("packaged dump has an unexpected size");
    }
    if (::fchmod(temporary.get(), originalStatus.st_mode & 0777) != 0)
    {
        throwSystemError("could not preserve dump archive permissions",
                         temporaryName);
    }
    if (::fsync(temporary.get()) != 0)
    {
        throwSystemError("could not synchronize packaged dump", temporaryName);
    }

    auto outputDirectory = openOutputDirectory(request.output);
    if (replacesSource)
    {
        if (::rename(temporaryName.c_str(), request.output.c_str()) != 0)
        {
            throwSystemError("could not publish packaged dump", request.output);
        }
    }
    else
    {
        publishNoReplace(temporaryName, request.output);
    }
    cleanup.release();
    // Rename is the publication commit point. Keep the descriptor open until
    // afterward so close-only watchers receive IN_CLOSE_WRITE for the completed
    // final name. A post-rename close or directory-fsync error cannot be rolled
    // back safely, so those operations are best effort after the content fsync.
    const auto unusedSync = ::fsync(outputDirectory.get());
    static_cast<void>(unusedSync);
    temporary.reset();
    source.reset();
}

} // namespace openpower::dump::header

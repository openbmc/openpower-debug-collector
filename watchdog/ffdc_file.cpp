#include "ffdc_file.hpp"

#include <errno.h>     // for errno
#include <fcntl.h>     // for open()
#include <string.h>    // for strerror()
#include <sys/stat.h>  // for open()
#include <sys/types.h> // for open()

#include <stdexcept>
#include <string>

namespace watchdog
{
namespace dump
{

FFDCFile::FFDCFile(const json& calloutDataObject) :
    calloutData(calloutDataObject.dump())
{
    prepareFFDCFile();
}

FFDCFile::~FFDCFile()
{
    remove();
}

void FFDCFile::prepareFFDCFile()
{
    createFile();
    writeCalloutData();
    seekFilePosition();
}

void FFDCFile::createFile()
{
    // Open the temporary file for both reading and writing
    int fd = open(tempFile.getPath().c_str(), O_RDWR);
    if (fd == -1)
    {
        throw std::runtime_error{std::string{"Unable to open FFDC file: "} +
                                 strerror(errno)};
    }

    // Store file descriptor in FileDescriptor object
    descriptor.set(fd);
}

void FFDCFile::writeCalloutData()
{
    ssize_t rc =
        write(getFileDescriptor(), calloutData.c_str(), calloutData.size());

    if (rc == -1)
    {
        log<level::ERR>(fmt::format("Failed to write callout info "
                                    "in file({}), errorno({}), errormsg({})",
                                    tempFile.getPath().c_str(), errno,
                                    strerror(errno))
                            .c_str());
        throw std::runtime_error("Failed to write phalPELCallouts info");
    }
    else if (rc != static_cast<ssize_t>(calloutData.size()))
    {
        log<level::WARNING>(fmt::format("Could not write all callout "
                                        "info in file({}), written byte({}) "
                                        "and total byte({})",
                                        tempFile.getPath().c_str(), rc,
                                        calloutData.size())
                                .c_str());
    }
}

void FFDCFile::seekFilePosition()
{
    int rc = lseek(getFileDescriptor(), 0, SEEK_SET);

    if (rc == -1)
    {
        log<level::ERR>(
            fmt::format("Failed to seek file postion to the beginning"
                        "in file({}), errorno({}) "
                        "and errormsg({})",
                        tempFile.getPath().c_str(), errno, strerror(errno))
                .c_str());
        throw std::runtime_error(
            "Failed to seek file postion to the beginning of the file");
    }
}

void FFDCFile::remove()
{
    // Close file descriptor.  Does nothing if descriptor was already closed.
    // Returns -1 if close failed.
    if (descriptor.close() == -1)
    {
        throw std::runtime_error{std::string{"Unable to close FFDC file: "} +
                                 strerror(errno)};
    }

    // Delete temporary file.  Does nothing if file was already deleted.
    // Throws an exception if the deletion failed.
    tempFile.remove();
}

} // namespace dump
} // namespace watchdog

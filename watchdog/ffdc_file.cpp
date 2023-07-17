#include "ffdc_file.hpp"

#include <errno.h>     // for errno
#include <fcntl.h>     // for open()
#include <string.h>    // for strerror()
#include <sys/stat.h>  // for open()
#include <sys/types.h> // for open()

#include <phosphor-logging/log.hpp>

#include <format>
#include <stdexcept>
#include <string>

namespace watchdog
{
namespace dump
{
using namespace phosphor::logging;

FFDCFile::FFDCFile(const json& calloutDataObject) :
    calloutData(calloutDataObject.dump())
{
    prepareFFDCFile();
}

FFDCFile::~FFDCFile()
{
    // Close file descriptor.  Does nothing if descriptor was already closed.
    if (descriptor.close() == -1)
    {
        log<level::ERR>(std::format("Unable to close FFDC file: errormsg({})",
                                    strerror(errno))
                            .c_str());
    }

    // Delete temporary file.  Does nothing if file was already deleted.
    tempFile.remove();
}

void FFDCFile::prepareFFDCFile()
{
    // Open the temporary file for both reading and writing
    int fd = open(tempFile.getPath().c_str(), O_RDWR);
    if (fd == -1)
    {
        throw std::runtime_error{std::string{"Unable to open FFDC file: "} +
                                 strerror(errno)};
    }

    ssize_t rc = write(fd, calloutData.c_str(), calloutData.size());

    if (rc == -1)
    {
        log<level::ERR>(std::format("Failed to write callout info "
                                    "in file({}), errorno({}), errormsg({})",
                                    tempFile.getPath().c_str(), errno,
                                    strerror(errno))
                            .c_str());
        throw std::runtime_error("Failed to write phalPELCallouts info");
    }
    else if (rc != static_cast<ssize_t>(calloutData.size()))
    {
        log<level::WARNING>(std::format("Could not write all callout "
                                        "info in file({}), written byte({}) "
                                        "and total byte({})",
                                        tempFile.getPath().c_str(), rc,
                                        calloutData.size())
                                .c_str());
    }

    int retCode = lseek(fd, 0, SEEK_SET);

    if (retCode == -1)
    {
        log<level::ERR>(
            std::format("Failed to seek file postion to the beginning"
                        "in file({}), errorno({}) "
                        "and errormsg({})",
                        tempFile.getPath().c_str(), errno, strerror(errno))
                .c_str());
        throw std::runtime_error(
            "Failed to seek file postion to the beginning of the file");
    }

    // Store file descriptor in FileDescriptor object
    descriptor.set(fd);
}

} // namespace dump
} // namespace watchdog

#include "ffdc_file.hpp"

#include <errno.h>     // for errno
#include <fcntl.h>     // for open()
#include <string.h>    // for strerror()
#include <sys/stat.h>  // for open()
#include <sys/types.h> // for open()

#include <phosphor-logging/lg2.hpp>

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
    // Close file descriptor.  Does nothing if descriptor was already closed.
    if (descriptor.close() == -1)
    {
        lg2::error("Unable to close FFDC file: errormsg({ERRORMSG})",
                   "ERRORMSG", strerror(errno));
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
        throw std::runtime_error{
            std::string{"Unable to open FFDC file: "} + strerror(errno)};
    }

    ssize_t rc = write(fd, calloutData.c_str(), calloutData.size());

    if (rc == -1)
    {
        lg2::error("Failed to write callout info in file({FILE}), "
                   "errorno({ERRNO}), errormsg({ERRORMSG})",
                   "FILE", tempFile.getPath(), "ERRNO", errno, "ERRORMSG",
                   strerror(errno));

        throw std::runtime_error("Failed to write phalPELCallouts info");
    }
    else if (rc != static_cast<ssize_t>(calloutData.size()))
    {
        lg2::warning("Could not write all callout info in file({FILE}), "
                     "written byte({WRITTEN}), total byte({TOTAL})",
                     "FILE", tempFile.getPath(), "WRITTEN", rc, "TOTAL",
                     calloutData.size());
    }

    int retCode = lseek(fd, 0, SEEK_SET);

    if (retCode == -1)
    {
        lg2::error("Failed to seek file position to the beginning in "
                   "file({FILE}), errorno({ERRNO}), errormsg({ERRORMSG})",
                   "FILE", tempFile.getPath(), "ERRNO", errno, "ERRORMSG",
                   strerror(errno));

        throw std::runtime_error(
            "Failed to seek file position to the beginning of the file");
    }

    // Store file descriptor in FileDescriptor object
    descriptor.set(fd);
}

} // namespace dump
} // namespace watchdog

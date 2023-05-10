#pragma once

#include <unistd.h> // for close()

namespace watchdog
{
namespace dump
{

/**
 * @class FileDescriptor
 *
 * This class manages an open file descriptor.
 *
 * The file descriptor can be closed by calling close().  Otherwise it will be
 * closed by the destructor.
 *
 * FileDescriptor objects cannot be copied, but they can be moved.  This enables
 * them to be stored in containers like std::vector.
 */
class FileDescriptor
{
  public:
    FileDescriptor() = default;
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    /**
     * @brief Constructor
     *
     * @param[in] fd - File descriptor
     */
    explicit FileDescriptor(int fd) : fd(fd) {}

    /**
     * @brief Move constructor.
     *
     * @details description ownership of a file descriptor.
     *
     * @param other - FileDescriptor object being moved
     */
    FileDescriptor(FileDescriptor&& other) : fd(other.fd)
    {
        other.fd = -1;
    }

    /**
     * @brief Move assignment operator.
     *
     * @details description the file descriptor owned by this object, if any.
     * Then transfers ownership of the file descriptor owned by the other
     * object.
     *
     * @param other - FileDescriptor object being moved
     */
    FileDescriptor& operator=(FileDescriptor&& other)
    {
        // Verify not assigning object to itself (a = std::move(a))
        if (this != &other)
        {
            set(other.fd);
            other.fd = -1;
        }
        return *this;
    }

    /**
     * @brief brief description.
     *
     * @details Closes the file descriptor if necessary.
     */
    ~FileDescriptor()
    {
        close();
    }

    /**
     * @brief Returns the file descriptor.
     *
     * @return File descriptor.  Returns -1 if this object does not contain an
     *         open file descriptor.
     */
    int operator()() const
    {
        return fd;
    }

    /**
     * @brief Check whether this object contains an open file descriptor.
     *
     * @return true if object contains an open file descriptor, false otherwise.
     */
    operator bool() const
    {
        return fd != -1;
    }

    /**
     * @brief Closes the file descriptor.
     *
     * @details Does nothing if the file descriptor was not set or was already
     * closed.
     *
     * @return 0 if descriptor was successfully closed.  Returns -1 if an error
     *         occurred; errno will be set appropriately.
     */
    int close()
    {
        int rc = 0;
        if (fd >= 0)
        {
            rc = ::close(fd);
            fd = -1;
        }
        return rc;
    }

    /**
     * @brief Sets the file descriptor.
     *
     * @details Closes the previous file descriptor if necessary.
     *
     * @param[in] descriptor - File descriptor
     */
    void set(int descriptor)
    {
        (void)close();
        fd = descriptor;
    }

  private:
    /**
     * @brief File descriptor.
     */
    int fd = -1;
};

} // namespace dump
} // namespace watchdog

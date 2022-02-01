#pragma once

#include "file_descriptor.hpp"
#include "utils.hpp"
#include "xyz/openbmc_project/Logging/Create/server.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>

namespace watchdog
{
namespace dump
{

namespace fs = std::filesystem;
using FFDCFormat =
    sdbusplus::xyz::openbmc_project::Logging::server::Create::FFDCFormat;
using FFDCTuple =
    std::tuple<FFDCFormat, uint8_t, uint8_t, sdbusplus::message::unix_fd>;

using ::nlohmann::json;
/**
 * @class FFDCFile
 *
 * File that contains FFDC (first failure data capture) data in json format.
 *
 * This class is used to store FFDC json callout data in an error log.
 */
class FFDCFile
{
  public:
    // Specify which compiler-generated methods we want
    FFDCFile() = delete;
    FFDCFile(const FFDCFile&) = delete;
    FFDCFile(FFDCFile&&) = default;
    FFDCFile& operator=(const FFDCFile&) = delete;
    FFDCFile& operator=(FFDCFile&&) = default;
    ~FFDCFile();

    /**
     * @brief Constructor
     *
     * @details Creates the FFDC file by using passed json data.
     *
     * Throws an exception if an error occurs.
     */
    explicit FFDCFile(const json& calloutData);

    /**
     * @brief Returns the file descriptor for the file.
     *
     * @details The file is open for both reading and writing.
     *
     * @return file descriptor
     */
    int getFileDescriptor() const
    {
        // Return the integer file descriptor within the FileDescriptor object
        return descriptor();
    }

    /**
     * @brief Returns the absolute path to the file.
     *
     * @return absolute path
     */
    const fs::path& getPath() const
    {
        return tempFile.getPath();
    }

  private:
    /**
     * @brief Temporary file where FFDC data is stored.
     *
     * @details The TemporaryFile destructor will automatically delete the file
     * if it was not explicitly deleted using remove().
     */
    TemporaryFile tempFile{};

    /**
     * @brief File descriptor for reading from/writing to the file.
     *
     * @details The FileDescriptor destructor will automatically close the file
     * if it was not explicitly closed using remove().
     */
    FileDescriptor descriptor{};

    /**
     * @brief Used to store callout ffdc data from passed json object
     */
    std::string calloutData;

    /**
     * @brief Creates FFDC file for creating PEL records.
     */
    void prepareFFDCFile();
};

} // namespace dump
} // namespace watchdog

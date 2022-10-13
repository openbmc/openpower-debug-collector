#pragma once

#include "xyz/openbmc_project/Logging/Entry/server.hpp"

#include <phal_exception.H>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
namespace openpower
{
namespace dump
{
namespace pel
{
using FFDCData = std::vector<std::pair<std::string, std::string>>;

using Severity = sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;

using json = nlohmann::json;

using namespace openpower::phal;

/**
 * @brief Create SBE boot error PEL and return id
 *
 * @param[in] event - the event type
 * @param[in] sbeError - SBE error object
 * @param[in] ffdcData - failure data to append to PEL
 * @param[in] severity - severity of the log
 * @return Platform log id
 */
uint32_t createSbeErrorPEL(const std::string& event, const sbeError_t& sbeError,
                           const FFDCData& ffdcData,
                           const Severity severity = Severity::Error);

/**
 * @class FFDCFile
 * @brief This class is used to create ffdc data file and to get fd
 */
class FFDCFile
{
  public:
    FFDCFile() = delete;
    FFDCFile(const FFDCFile&) = delete;
    FFDCFile& operator=(const FFDCFile&) = delete;
    FFDCFile(FFDCFile&&) = delete;
    FFDCFile& operator=(FFDCFile&&) = delete;

    /**
     * Used to pass json object to create unique ffdc file by using
     * passed json data.
     */
    explicit FFDCFile(const json& pHALCalloutData);

    /**
     * Used to remove created ffdc file.
     */
    ~FFDCFile();

    /**
     * Used to get created ffdc file file descriptor id.
     *
     * @return file descriptor id
     */
    int getFileFD() const;

  private:
    /**
     * Used to store callout ffdc data from passed json object.
     */
    std::string calloutData;

    /**
     * Used to store unique ffdc file name.
     */
    std::string calloutFile;

    /**
     * Used to store created ffdc file descriptor id.
     */
    int fileFD;

    /**
     * Used to create ffdc file to pass PEL api for creating
     * pel records.
     *
     * @return NULL
     */
    void prepareFFDCFile();

    /**
     * Create unique ffdc file.
     *
     * @return NULL
     */
    void createCalloutFile();

    /**
     * Used write json object value into created file.
     *
     * @return NULL
     */
    void writeCalloutData();

    /**
     * Used set ffdc file seek position begining to consume by PEL
     *
     * @return NULL
     */
    void setCalloutFileSeekPos();

    /**
     * Used to remove created ffdc file.
     *
     * @return NULL
     */
    void removeCalloutFile();

}; // FFDCFile end

} // namespace pel
} // namespace dump
} // namespace openpower

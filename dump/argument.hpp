#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace openpower
{
namespace dump
{
namespace util
{

/**
 * @class ArgumentParser
 * @brief Parses and stores command-line arguments.
 *
 * This class is designed to parse command-line arguments and provide
 * easy access to the values associated with specific options.
 */
class ArgumentParser
{
  public:
    /**
     * @brief Constructs an ArgumentParser and parses command-line arguments.
     * @param[in] argc - Argument count.
     * @param[in] argv - Argument vector.
     *
     * The constructor takes the standard command-line arguments `argc` and
     * `argv` as input and parses them according to specified options. The
     * parsing results are stored internally and can be accessed via member
     * functions.
     */
    ArgumentParser(int argc, char** argv);

    /**
     * @brief Retrieves the string value associated with a given option.
     * @param[in] option - The option key to search for.
     * @return An std::optional containing the string value if the option
     *         was found and has an associated string value; std::nullopt
     * otherwise.
     */
    std::optional<std::string> getString(const std::string& option) const;

    /**
     * @brief Retrieves the integer value associated with a given option.
     * @param[in] option - The option key to search for.
     * @return An std::optional containing the integer value if the option
     *         was found and can be converted to an integer; std::nullopt
     * otherwise.
     */
    std::optional<int> getInt(const std::string& option) const;

    /**
     * @brief Checks whether a flag (boolean option) was set.
     * @param[in] option - The flag option key to check.
     * @return True if the option was set, false otherwise.
     */
    bool getBool(const std::string& option) const;

    /**
     * @brief Prints usage information for the program.
     * @param[in] programName - The name of the program.
     *
     * This static function can be used to print a standard usage message
     * that describes the supported command-line options.
     */
    static void usage(const std::string& programName);

  private:
    /**
     * @brief Parses command-line arguments.
     * @param[in] argc - Argument count.
     * @param[in] argv - Argument vector.
     *
     * Parses the command-line arguments provided to the constructor and
     * stores the results internally for later retrieval.
     */
    void parseArguments(int argc, char** argv);

    // Stores parsed command-line arguments.
    std::unordered_map<std::string, std::variant<std::string, bool>> arguments;
};

} // namespace util
} // namespace dump
} // namespace openpower

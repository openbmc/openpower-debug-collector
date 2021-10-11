#pragma once

#include <string>

namespace watchdog
{
namespace dump
{

/**
 * @brief Transition the host state
 *
 * @param target - the target to transition the host to
 */
void transitionHost(const std::string& target);

} // namespace dump
} // namespace watchdog

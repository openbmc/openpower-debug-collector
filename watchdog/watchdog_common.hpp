#pragma once

#include <string>
extern "C"
{
#include <libpdbg.h>
}

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

/** @brief Read state of autoreboot property via dbus */
bool isAutoRebootEnabled();

} // namespace dump
} // namespace watchdog

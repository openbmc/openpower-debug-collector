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

/**
 *  @brief  Check if primary processor or not
 *
 *  @param procTarget - Processor target to check if primary or not
 *
 *  @return True/False
 */
bool isPrimaryProc(struct pdbg_target* procTarget);

/**
 * @brief Helper function to set PDBG_DTB
 *
 * PDBG_DTB environment variable set to CEC device tree path
 */
void setDevtreeEnv();

/** @brief initialize pdbg target
 *
 * Exceptions: runtime_error
 */
void pdbgInit();

/**
 *  @brief  Helper function to find PIB target needed for PIB operations
 *
 *  @param[in]  procTarget - Processor target to find the PIB target on
 *
 *  @return Valid pointer to PIB target on success
 *
 *  Exceptions: runtime_error
 */
struct pdbg_target* getPibTarget(struct pdbg_target* procTarget);

} // namespace dump
} // namespace watchdog

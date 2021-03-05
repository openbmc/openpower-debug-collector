#pragma once

#include <sdbusplus/server.hpp>
#include <string>

/**
 * @brief Get DBUS service for input interface via mapper call
 *
 * This is an internal function to be used only by functions within this
 * file.
 *
 * @param[in] bus -  DBUS Bus Object
 * @param[in] intf - DBUS Interface
 * @param[in] path - DBUS Object Path
 *
 * @return distinct dbus name for input interface/path
 **/
std::string getService(sdbusplus::bus::bus& bus, const std::string& intf,
                       const std::string& path);

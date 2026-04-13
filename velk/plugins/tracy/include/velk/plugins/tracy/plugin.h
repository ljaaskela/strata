#ifndef VELK_PLUGINS_TRACY_PLUGIN_H
#define VELK_PLUGINS_TRACY_PLUGIN_H

#include <velk/common.h>

namespace velk {

namespace ClassId {
/** @brief Tracy perf sink. */
inline constexpr Uid TracyPerfSink{"457acc71-75e6-43f7-8287-491c44345660"};
} // namespace ClassId

namespace PluginId {
/** @brief Tracy profiler plugin (velk_tracy). */
inline constexpr Uid TracyPlugin{"03f099a0-7997-4859-866a-03fa310356ff"};
} // namespace PluginId

} // namespace velk

#endif // VELK_PLUGINS_TRACY_PLUGIN_H

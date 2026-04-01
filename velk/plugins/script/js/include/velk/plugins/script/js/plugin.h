#ifndef VELK_PLUGINS_SCRIPT_JS_PLUGIN_H
#define VELK_PLUGINS_SCRIPT_JS_PLUGIN_H

#include <velk/common.h>

namespace velk {

namespace ClassId {
/** @brief JS import handler (processes "scripts" JSON collection). */
inline constexpr Uid JsImportHandler{"c8f4a2b3-5d6e-7f90-ab1c-2d3e4f5a6b7c"};
} // namespace ClassId

namespace PluginId {
/** @brief JavaScript scripting plugin (velk_js). */
inline constexpr Uid JsPlugin{"d9a5b3c4-6e7f-8a01-bc2d-3e4f5a6b7c8d"};
} // namespace PluginId

} // namespace velk

#endif // VELK_PLUGINS_SCRIPT_JS_PLUGIN_H

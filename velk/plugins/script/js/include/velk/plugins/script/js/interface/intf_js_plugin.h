#ifndef VELK_INTF_JS_PLUGIN_H
#define VELK_INTF_JS_PLUGIN_H

#include <velk/interface/intf_interface.h>

namespace velk {

/**
 * @brief Public interface for the JS scripting plugin.
 *
 * Allows programmatic script registration beyond JSON import.
 * Chain: IInterface -> IJsPlugin
 */
class IJsPlugin : public Interface<IJsPlugin>
{
public:
};

} // namespace velk

#endif // VELK_INTF_JS_PLUGIN_H

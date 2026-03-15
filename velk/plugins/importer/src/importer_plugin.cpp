#include "importer_plugin.h"

namespace velk {

ReturnValue ImporterPlugin::initialize(IVelk& velk, PluginConfig& config)
{
    return ReturnValue::Success;
}

ReturnValue ImporterPlugin::shutdown(IVelk&)
{
    return ReturnValue::Success;
}

} // namespace velk

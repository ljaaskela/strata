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

ImportResult ImporterPlugin::import_from_json(string_view json) const
{
    return importer_.import_from_json(json);
}

void ImporterPlugin::register_class_alias(string_view alias, Uid class_uid)
{
    importer_.register_class_alias(alias, class_uid);
}

} // namespace velk

#include "importer_plugin.h"
#include "json_importer.h"

#include <velk/string.h>

namespace velk {

ReturnValue ImporterPlugin::initialize(IVelk& velk, PluginConfig&)
{
    return ::velk::register_type<JsonImporter>(velk);
}

ReturnValue ImporterPlugin::shutdown(IVelk&)
{
    return ReturnValue::Success;
}

void ImporterPlugin::register_class_alias(string_view alias, Uid class_uid)
{
    aliases_[std::string(alias.data(), alias.size())] = class_uid;
}

Uid ImporterPlugin::resolve_class(string_view name) const
{
    std::string name_str(name.data(), name.size());

    // Try UUID format first
    if (is_valid_uid_format(name)) {
        Uid uid(name);
        if (::velk::instance().type_registry().get_class_info(uid)) {
            return uid;
        }
    }

    // Try registered aliases
    auto it = aliases_.find(name_str);
    if (it != aliases_.end()) {
        return it->second;
    }

    // Look up by registered class name
    return ::velk::instance().type_registry().find_class_by_name(name);
}

} // namespace velk

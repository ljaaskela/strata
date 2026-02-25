#ifndef VELK_PLUGINS_HIVE_PLUGIN_H
#define VELK_PLUGINS_HIVE_PLUGIN_H

#include <velk/ext/plugin.h>
#include <velk/interface/hive/intf_hive_store.h>

namespace velk {

/**
 * @brief Built-in plugin that registers hive types (HiveStore, Hive).
 *
 * After loading this plugin, applications can create HiveStore instances
 * via velk.create(ClassId::HiveStore).
 */
class HivePlugin : public ext::Plugin<HivePlugin>
{
public:
    VELK_CLASS_UID(ClassId::HivePlugin);
    VELK_PLUGIN_NAME("HivePlugin");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override;
    ReturnValue shutdown(IVelk& velk) override;
};

} // namespace velk

#endif // VELK_PLUGINS_HIVE_PLUGIN_H

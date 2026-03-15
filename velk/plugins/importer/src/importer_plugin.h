#ifndef VELK_IMPORTER_PLUGIN_H
#define VELK_IMPORTER_PLUGIN_H

#include <velk/ext/plugin.h>
#include <velk/plugins/importer/interface/intf_importer_plugin.h>

namespace velk {

class ImporterPlugin final : public ext::Plugin<ImporterPlugin, IImporterPlugin>
{
public:
    VELK_PLUGIN_UID("51952e9e-6802-4ebf-acff-c742ecd079fe");
    VELK_PLUGIN_NAME("importer");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override;
    ReturnValue shutdown(IVelk&) override;
};

} // namespace velk

VELK_PLUGIN(velk::ImporterPlugin)

#endif // VELK_IMPORTER_PLUGIN_H

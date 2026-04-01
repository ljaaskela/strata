#ifndef VELK_JS_PLUGIN_H
#define VELK_JS_PLUGIN_H

#include "js_context.h"
#include "js_import_handler.h"

#include <velk/ext/plugin.h>
#include <velk/plugins/script/js/interface/intf_js_plugin.h>
#include <velk/plugins/script/js/plugin.h>

extern "C" {
#include <quickjs.h>
}

#include <memory>
#include <unordered_map>

namespace velk {

class JsPlugin final : public ext::Plugin<JsPlugin, IJsPlugin>
{
public:
    VELK_PLUGIN_UID("d9a5b3c4-6e7f-8a01-bc2d-3e4f5a6b7c8d");
    VELK_PLUGIN_NAME("js");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override;
    ReturnValue shutdown(IVelk&) override;
    void post_update(const IPlugin::PostUpdateInfo& info) override;

    /** @brief Returns or creates a JS context for the given store. */
    JsStoreContext& get_or_create_context(IStore& store);

private:
    JSRuntime* rt_{};
    std::unordered_map<IStore*, std::unique_ptr<JsStoreContext>> contexts_;
};

} // namespace velk

#endif // VELK_JS_PLUGIN_H

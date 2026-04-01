#include "js_plugin.h"

#include "js_proxy.h"

namespace velk {

ReturnValue JsPlugin::initialize(IVelk& velk, PluginConfig& config)
{
    rt_ = JS_NewRuntime();
    if (!rt_) {
        return ReturnValue::Fail;
    }

    JsProxy::register_class(rt_);

    config.enableUpdate = true;

    auto rv = ::velk::register_type<JsImportHandler>(velk);
    return rv;
}

ReturnValue JsPlugin::shutdown(IVelk&)
{
    // Destroy all contexts before the runtime
    contexts_.clear();

    if (rt_) {
        JS_FreeRuntime(rt_);
        rt_ = nullptr;
    }
    return ReturnValue::Success;
}

void JsPlugin::post_update(const IPlugin::PostUpdateInfo&)
{
    if (rt_) {
        JS_RunGC(rt_);
    }
}

JsStoreContext& JsPlugin::get_or_create_context(IStore& store)
{
    auto it = contexts_.find(&store);
    if (it != contexts_.end()) {
        return *it->second;
    }
    auto ctx = std::make_unique<JsStoreContext>(rt_, store);
    auto& ref = *ctx;
    contexts_[&store] = std::move(ctx);
    return ref;
}

} // namespace velk

VELK_PLUGIN(velk::JsPlugin)

#ifndef VELK_TRACY_PLUGIN_H
#define VELK_TRACY_PLUGIN_H

#include <velk/ext/plugin.h>
#include <velk/interface/intf_perf_log.h>

namespace velk {

class TracyPlugin final : public ext::Plugin<TracyPlugin>
{
public:
    VELK_PLUGIN_UID("03f099a0-7997-4859-866a-03fa310356ff");
    VELK_PLUGIN_NAME("tracy");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override;
    ReturnValue shutdown(IVelk& velk) override;

private:
    IPerfSink::Ptr sink_;
};

} // namespace velk

VELK_PLUGIN(velk::TracyPlugin)

#endif // VELK_TRACY_PLUGIN_H

#include "tracy_plugin.h"
#include "tracy_perf_sink.h"

namespace velk {

ReturnValue TracyPlugin::initialize(IVelk& velk, PluginConfig&)
{
    sink_ = ext::make_object<impl::TracyPerfSink, IPerfSink>();
    velk.perf_log().set_perf_sink(sink_);
    return ReturnValue::Success;
}

ReturnValue TracyPlugin::shutdown(IVelk& velk)
{
    velk.perf_log().set_perf_sink({});
    sink_.reset();
    return ReturnValue::Success;
}

} // namespace velk

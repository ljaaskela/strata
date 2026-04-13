#ifndef VELK_TRACY_PERF_SINK_H
#define VELK_TRACY_PERF_SINK_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_perf_log.h>

namespace velk::impl {

class TracyPerfSink final : public ext::ObjectCore<TracyPerfSink, IPerfSink>
{
public:
    VELK_CLASS_UID("457acc71-75e6-43f7-8287-491c44345660");

    void start_perf(uint64_t key, string_view label,
                    const char* file, uint32_t line) override;
    void end_perf(uint64_t key, string_view label, Duration elapsed) override;
    void event(PerfEvent type) override;
};

} // namespace velk::impl

#endif // VELK_TRACY_PERF_SINK_H

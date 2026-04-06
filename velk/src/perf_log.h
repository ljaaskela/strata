#ifndef VELK_PERF_LOG_H
#define VELK_PERF_LOG_H

#include <velk/ext/interface_dispatch.h>
#include <velk/interface/intf_perf_log.h>
#include <velk/vector.h>

#include <mutex>

namespace velk {

/**
 * @brief Concrete implementation of IPerfLog.
 *
 * Owned as a flat member of VelkInstance. Collects scoped timing
 * measurements and dispatches results to an optional IPerfSink.
 */
class PerfLog final : public ext::InterfaceDispatch<IPerfLog>
{
public:
    PerfLog();

    void start_perf(uint64_t key, string_view label) override;
    Duration end_perf(uint64_t key) override;
    Duration get_perf(uint64_t key) const override;
    void set_perf_sink(const IPerfSink::Ptr& sink) override;

private:
    struct PerfEntry
    {
        uint64_t key = 0;
        string_view label;
        int64_t start_us = 0;
    };

    mutable vector<PerfEntry> perf_entries_;
    mutable std::mutex perf_mutex_;
    IPerfSink::Ptr perf_sink_;
};

} // namespace velk

#endif // VELK_PERF_LOG_H

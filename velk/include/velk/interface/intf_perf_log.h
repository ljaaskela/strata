#ifndef VELK_INTF_PERF_LOG_H
#define VELK_INTF_PERF_LOG_H

#include <velk/duration.h>
#include <velk/interface/intf_interface.h>
#include <velk/string_view.h>

#include <cstdint>

namespace velk {

/**
 * @brief Sink interface for receiving performance measurement results.
 *
 * Implement this interface and pass it to IPerfLog::set_perf_sink() to receive
 * structured timing data. Called from end_perf() on the thread that ends
 * the measurement.
 */
class IPerfSink : public Interface<IPerfSink>
{
public:
    /**
     * @brief Called when a performance measurement completes.
     * @param key     The hash key identifying the measurement region.
     * @param label   Human-readable label (may be empty if start_perf was called with key only).
     * @param elapsed Duration of the measured region.
     */
    virtual void write_perf(uint64_t key, string_view label, Duration elapsed) = 0;
};

/**
 * @brief Performance logging interface exposed by the velk instance.
 *
 * Retrieved via instance().perf_log(). Provides scoped timing measurements
 * with pluggable sink for collecting results.
 */
class IPerfLog : public Interface<IPerfLog>
{
public:
    /** @brief Begins a performance measurement for the given key. */
    virtual void start_perf(uint64_t key, string_view label = {}) = 0;
    /** @brief Ends a performance measurement and reports to the perf sink. Returns elapsed duration. */
    virtual Duration end_perf(uint64_t key) = 0;
    /** @brief Returns elapsed time for a running measurement. Returns zero if key not found. */
    virtual Duration get_perf(uint64_t key) const = 0;
    /** @brief Sets the performance sink. Pass an empty pointer to disable perf reporting. */
    virtual void set_perf_sink(const IPerfSink::Ptr& sink) = 0;
};

} // namespace velk

#endif // VELK_INTF_PERF_LOG_H

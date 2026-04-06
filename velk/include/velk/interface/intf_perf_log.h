#ifndef VELK_INTF_PERF_LOG_H
#define VELK_INTF_PERF_LOG_H

#include <velk/duration.h>
#include <velk/interface/intf_interface.h>
#include <velk/string.h>
#include <velk/string_view.h>
#include <velk/vector.h>

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

/** @brief Accumulated statistics for a single perf key. */
struct PerfStats
{
    static constexpr size_t kSampleCapacity = 128;

    uint64_t key{};     ///< Measurement region key (usually an FNV-1a 64-bit hash of label).
    string label;       ///< Human-readable name of the region.
    Duration last;      ///< Duration of the most recent measurement.
    Duration min;       ///< Shortest measurement seen.
    Duration max;       ///< Longest measurement seen.
    Duration total;     ///< Sum of all measurements.
    Duration avg;       ///< Running average (total / count).
    Duration median;    ///< Median of recent samples.
    Duration p95;       ///< 95th percentile of recent samples.
    uint32_t count = 0; ///< Number of completed measurements.

    /** @brief Circular buffer of recent samples for percentile computation. */
    Duration samples[kSampleCapacity]{};
    uint32_t sample_count = 0; ///< Number of samples stored (up to kSampleCapacity).
    uint32_t sample_pos = 0;   ///< Next write position in the circular buffer.
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

    /** @brief Returns a snapshot of accumulated stats for all keys. */
    virtual vector<PerfStats> get_stats() const = 0;
    /** @brief Resets all accumulated stats. */
    virtual void reset_stats() = 0;
    /** @brief Enables or disables stats accumulation. Enabled by default. */
    virtual void set_stats_enabled(bool enabled) = 0;
};

} // namespace velk

#endif // VELK_INTF_PERF_LOG_H

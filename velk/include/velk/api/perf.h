#ifndef VELK_API_PERF_H
#define VELK_API_PERF_H

#include <velk/api/velk.h>
#include <velk/uid.h>

namespace velk {

/**
 * @brief RAII helper that measures the lifetime of a scope.
 *
 * Calls start_perf on construction and end_perf on destruction.
 * Header-only; does not cross the DLL boundary.
 *
 * Usage:
 * @code
 * {
 *     velk::PerfScope ps("rebuild_batches");
 *     // ... work ...
 * } // end_perf called, result sent to perf sink
 * @endcode
 */
struct PerfScope
{
    uint64_t key;

    /** @brief Constructs from a string label. Hashes the label to produce the key. */
    PerfScope(string_view label) : key(make_hash64(label))
    {
        instance().perf_log().start_perf(key, label);
    }

    /** @brief Constructs from a pre-computed key (no label forwarded). */
    explicit PerfScope(uint64_t k) : key(k)
    {
        instance().perf_log().start_perf(key);
    }

    ~PerfScope() { instance().perf_log().end_perf(key); }

    /** @brief Returns elapsed time since the scope started. */
    Duration elapsed() const { return instance().perf_log().get_perf(key); }

    PerfScope(const PerfScope&) = delete;
    PerfScope& operator=(const PerfScope&) = delete;
};

#define VELK_PERF_CONCAT_(a, b) a##b
#define VELK_PERF_CONCAT(a, b) VELK_PERF_CONCAT_(a, b)

/**
 * @def VELK_PERF_SCOPE(label)
 * @brief Creates a PerfScope with an auto-generated variable name.
 */
#define VELK_PERF_SCOPE(label) \
    ::velk::PerfScope VELK_PERF_CONCAT(_velk_perf_, __LINE__)(label)

} // namespace velk

#endif // VELK_API_PERF_H

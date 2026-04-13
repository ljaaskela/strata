#ifndef VELK_API_PERF_H
#define VELK_API_PERF_H

#include <velk/api/velk.h>
#include <velk/uid.h>

// VELK_PERF_ENABLED is set by CMake (VELK_ENABLE_PERF option).
// Default to enabled if not defined (e.g. standalone header inclusion).
#ifndef VELK_PERF_ENABLED
#define VELK_PERF_ENABLED 1
#endif

namespace velk {

#if VELK_PERF_ENABLED

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
    /** @brief Constructs from a string label with optional source location. */
    PerfScope(string_view label, const char* file = nullptr, uint32_t line = 0)
        : key(make_hash64(label))
    {
        instance().perf_log().start_perf(key, label, file, line);
    }

    /** @brief Constructs from a pre-computed key (no label forwarded). */
    explicit PerfScope(uint64_t k) : key(k) { instance().perf_log().start_perf(key); }

    ~PerfScope() { instance().perf_log().end_perf(key); }

    /** @brief Returns elapsed time since the scope started. */
    Duration elapsed() const { return instance().perf_log().get_perf(key); }

    PerfScope(const PerfScope&) = delete;
    PerfScope& operator=(const PerfScope&) = delete;

private:
    uint64_t key;
};

#define VELK_PERF_CONCAT_(a, b) a##b
#define VELK_PERF_CONCAT(a, b) VELK_PERF_CONCAT_(a, b)

/**
 * @def VELK_PERF_SCOPE(label)
 * @brief Creates a PerfScope with an auto-generated variable name.
 */
#define VELK_PERF_SCOPE(label) \
    ::velk::PerfScope VELK_PERF_CONCAT(_velk_perf_, __LINE__)(label, __FILE__, __LINE__)

/**
 * @def VELK_PERF_EVENT(type)
 * @brief Emits a performance event (e.g. frame boundaries).
 */
#define VELK_PERF_EVENT(type) ::velk::instance().perf_log().event(::velk::PerfEvent::type)

#else // !VELK_PERF_ENABLED

/** @brief No-op PerfScope when perf logging is disabled at compile time. */
struct PerfScope
{
    PerfScope(string_view, const char* = nullptr, uint32_t = 0) {}
    explicit PerfScope(uint64_t) {}
    ~PerfScope() = default;
    Duration elapsed() const { return {}; }
    PerfScope(const PerfScope&) = delete;
    PerfScope& operator=(const PerfScope&) = delete;
};

#define VELK_PERF_CONCAT_(a, b) a##b
#define VELK_PERF_CONCAT(a, b) VELK_PERF_CONCAT_(a, b)
#define VELK_PERF_SCOPE(label) \
    ::velk::PerfScope VELK_PERF_CONCAT(_velk_perf_, __LINE__)(label, __FILE__, __LINE__)
#define VELK_PERF_EVENT(type) ((void)0)

#endif // VELK_PERF_ENABLED

} // namespace velk

#endif // VELK_API_PERF_H

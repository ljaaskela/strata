#include "perf_log.h"

#include <velk/api/velk.h>

#include <chrono>

namespace velk {

static int64_t now_us()
{
    using clock = std::chrono::high_resolution_clock;
    return std::chrono::duration_cast<std::chrono::microseconds>(
               clock::now().time_since_epoch())
        .count();
}

PerfLog::PerfLog()
{
    perf_entries_.reserve(16);
}

void PerfLog::start_perf(uint64_t key, string_view label)
{
    auto now = now_us();
    std::lock_guard<std::mutex> lock(perf_mutex_);
    for (auto& e : perf_entries_) {
        if (e.key == key) {
            // Restart scope
            e.label = label;
            e.start_us = now;
            return;
        }
    }
    PerfEntry e;
    e.key = key;
    e.label = label;
    e.start_us = now;
    perf_entries_.emplace_back(std::move(e));
}

Duration PerfLog::end_perf(uint64_t key)
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    for (auto e = perf_entries_.begin(); e != perf_entries_.end(); e++) {
        if (e->key == key) {
            auto elapsed = Duration::from_microseconds(now_us() - e->start_us);
            auto label = e->label;
            if (perf_sink_) {
                perf_sink_->write_perf(key, label, elapsed);
            }
            perf_entries_.erase(e);
            return elapsed;
        }
    }
    VELK_LOG(W, "Invalid perf log key: %zu", key);
    return {};
}

Duration PerfLog::get_perf(uint64_t key) const
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    for (auto& e : perf_entries_) {
        if (e.key == key) {
            return Duration::from_microseconds(now_us() - e.start_us);
        }
    }
    VELK_LOG(W, "Invalid perf log key: %zu", key);
    return {};
}

void PerfLog::set_perf_sink(const IPerfSink::Ptr& sink)
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    perf_sink_ = sink;
}

} // namespace velk

#include "perf_log.h"

#include <velk/api/velk.h>

#include <chrono>

namespace velk {

static Duration now()
{
    using clock = std::chrono::high_resolution_clock;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(clock::now().time_since_epoch()).count();
    return Duration::from_microseconds(us);
}

PerfLog::PerfLog()
{
    // Initially reserve space for 16 parallel perf logs
    perf_entries_.reserve(16);
}

void PerfLog::start_perf(uint64_t key, string_view label)
{
    auto start = now();
    std::lock_guard<std::mutex> lock(perf_mutex_);
    for (auto& e : perf_entries_) {
        if (e.key == key) {
            e.label = label;
            e.start = start;
            return;
        }
    }
    PerfEntry e;
    e.key = key;
    e.label = label;
    e.start = start;
    perf_entries_.emplace_back(std::move(e));
}

Duration PerfLog::end_perf(uint64_t key)
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    for (auto e = perf_entries_.begin(); e != perf_entries_.end(); e++) {
        if (e->key == key) {
            auto elapsed = now() - e->start;
            auto label = e->label;
            if (stats_enabled_) {
                accumulate(key, label, elapsed);
            }
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

void PerfLog::accumulate(uint64_t key, string_view label, Duration elapsed)
{
    for (auto& s : stats_entries_) {
        if (s.key == key) {
            s.last = elapsed;
            if (elapsed < s.min) {
                s.min = elapsed;
            }
            if (elapsed > s.max) {
                s.max = elapsed;
            }
            s.total += elapsed;
            s.count++;
            s.avg = Duration::from_microseconds(s.total.to_microseconds() / s.count);
            return;
        }
    }
    PerfStats s;
    s.key = key;
    s.label = label;
    s.last = elapsed;
    s.min = elapsed;
    s.max = elapsed;
    s.total = elapsed;
    s.avg = elapsed;
    s.count = 1;
    stats_entries_.emplace_back(std::move(s));
}

Duration PerfLog::get_perf(uint64_t key) const
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    for (auto& e : perf_entries_) {
        if (e.key == key) {
            return now() - e.start;
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

vector<PerfStats> PerfLog::get_stats() const
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    return stats_entries_;
}

void PerfLog::reset_stats()
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    stats_entries_.clear();
}

void PerfLog::set_stats_enabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    stats_enabled_ = enabled;
}

} // namespace velk

#include "dependency_tracker.h"

#include <velk/interface/intf_object.h>

namespace velk::detail {

static thread_local DependencyTracker* tls_tracker = nullptr;

void record_dependency(const IProperty* prop)
{
    auto* t = tls_tracker;
    if (!(t && prop)) {
        return;
    }
    // Avoid duplicates (small N, linear scan is fine)
    for (auto& d : t->deps) {
        if (d.get() == prop) {
            return;
        }
    }
    // Get a ref-counted pointer via the object's get_self + interface cast
    const auto* obj = interface_cast<const IObject>(prop);
    if (auto p = get_self<IProperty>(obj)) {
        t->deps.push_back(p);
    }
}

DependencyTracker* push_tracker(DependencyTracker* tracker)
{
    auto* prev = tls_tracker;
    tls_tracker = tracker;
    return prev;
}

void pop_tracker(DependencyTracker* previous)
{
    tls_tracker = previous;
}

} // namespace velk::detail

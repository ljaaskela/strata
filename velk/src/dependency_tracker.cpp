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
    for (auto* d : t->deps) {
        if (d == prop) {
            return;
        }
    }
    t->deps.push_back(prop);
}

vector<IProperty::ConstWeakPtr> DependencyTracker::acquire() const
{
    vector<IProperty::ConstWeakPtr> result;
    result.reserve(deps.size());
    for (auto* raw : deps) {
        if (auto* obj = interface_cast<const IObject>(raw)) {
            result.emplace_back(obj->get_self<IProperty>());
        }
    }
    return result;
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

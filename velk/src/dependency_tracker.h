#ifndef VELK_DEPENDENCY_TRACKER_H
#define VELK_DEPENDENCY_TRACKER_H

#include <velk/interface/intf_property.h>
#include <velk/vector.h>

namespace velk::detail {

/** @brief Collects IProperty dependencies during binding evaluation.
 *  Stores raw pointers for fast diff against existing deps.
 *  Ref-counted pointers are only acquired when deps actually change. */
struct DependencyTracker
{
    vector<const IProperty*> deps;

    /** @brief Converts raw pointers to weak pointers. */
    vector<IProperty::ConstWeakPtr> acquire() const;
};

/** @brief Records the property as a dependency if a tracker is active. */
void record_dependency(const IProperty* prop);

/** @brief Sets the active tracker for the current thread. Returns the previous one. */
DependencyTracker* push_tracker(DependencyTracker* tracker);

/** @brief Restores the previous tracker. */
void pop_tracker(DependencyTracker* previous);

/** @brief RAII guard for push/pop tracker. */
struct TrackerScope
{
    DependencyTracker tracker;
    DependencyTracker* previous;

    TrackerScope() : previous(push_tracker(&tracker)) {}
    ~TrackerScope() { pop_tracker(previous); }

    TrackerScope(const TrackerScope&) = delete;
    TrackerScope& operator=(const TrackerScope&) = delete;
};

} // namespace velk::detail

#endif // VELK_DEPENDENCY_TRACKER_H

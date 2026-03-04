#ifndef HIERARCHY_H
#define HIERARCHY_H

#include <velk/ext/object.h>
#include <velk/interface/intf_hierarchy.h>

#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace velk {

/**
 * @brief Default implementation of IHierarchy backed by an unordered_map.
 *
 * Thread-safe: reads use shared locks, mutations use exclusive locks.
 * Listener callbacks and events are invoked outside the lock.
 */
class HierarchyImpl final : public ext::Object<HierarchyImpl, IHierarchy>
{
public:
    VELK_CLASS_UID(ClassId::Hierarchy);

    ReturnValue set_root(const IObject::Ptr& root) override;
    ReturnValue add(const IObject::Ptr& parent, const IObject::Ptr& child) override;
    ReturnValue insert(const IObject::Ptr& parent, size_t index, const IObject::Ptr& child) override;
    ReturnValue remove(const IObject::Ptr& object) override;
    ReturnValue replace(const IObject::Ptr& old_child, const IObject::Ptr& new_child) override;
    void clear() override;

    IObject::Ptr root() const override;
    HierarchyNode node_of(const IObject::Ptr& object) const override;
    IObject::Ptr parent_of(const IObject::Ptr& object) const override;
    vector<IObject::Ptr> children_of(const IObject::Ptr& object) const override;
    IObject::Ptr child_at(const IObject::Ptr& object, size_t index) const override;
    size_t child_count(const IObject::Ptr& object) const override;
    void for_each_child(const IObject::Ptr& object, void* context, ChildVisitorFn visitor) const override;
    bool contains(const IObject::Ptr& object) const override;
    size_t size() const override;

private:
    // Per-node storage: owning pointer, raw parent backlink, ordered children.
    struct Entry
    {
        IObject::Ptr object;                // Owning reference to the node's object.
        IObject* parent = nullptr;          // Raw backlink (non-owning); null for root.
        std::vector<IObject::Ptr> children; // Ordered child list.
    };

    // Recursively erases obj and all descendants from entries_, collecting them in removed.
    void remove_recursive(IObject* obj, std::vector<IObject::Ptr>& removed);
    // Collects all entry objects into out (unordered). Used before clear/set_root.
    void collect_all(std::vector<IObject::Ptr>& out) const;
    // Returns the owning Ptr of obj's parent, or null. Caller must hold a lock.
    IObject::Ptr lookup_parent(IObject* obj) const;
    // Fires on_hierarchy_left on each removed object that implements IHierarchyAware.
    void notify_left(const std::vector<IObject::Ptr>& removed);
    // Fires on_changing or on_changed if handlers exist. Invoked outside the lock.
    void fire_event(string_view name, HierarchyChange change);

    mutable std::shared_mutex mutex_;             // Shared for reads, exclusive for mutations.
    IObject::Ptr root_;                           // Root object, or null if empty.
    std::unordered_map<IObject*, Entry> entries_; // All nodes keyed by raw pointer.
};

} // namespace velk

#endif // HIERARCHY_H

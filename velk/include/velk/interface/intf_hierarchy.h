#ifndef VELK_INTF_HIERARCHY_H
#define VELK_INTF_HIERARCHY_H

#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object.h>
#include <velk/vector.h>

namespace velk {

class IHierarchy;

/** @brief Lightweight handle binding an object to its hierarchy. */
struct HierarchyNode
{
    IObject::Ptr object;            ///< The object this node represents.
    weak_ptr<IHierarchy> hierarchy; ///< Weak reference to the owning hierarchy.
};

/** @brief Describes a hierarchy mutation for on_changing/on_changed events. */
struct HierarchyChange
{
    enum class Type : uint8_t
    {
        SetRoot, ///< Root is being set. child = new root.
        Add,     ///< Child appended. parent, child set.
        Insert,  ///< Child inserted at index. parent, child, index set.
        Remove,  ///< Subtree removed. parent, child set. child is subtree root.
        Replace, ///< In-place replacement. parent, child (new), old_child set.
        Clear    ///< All objects removed.
    };

    Type type{};
    weak_ptr<IHierarchy> hierarchy; ///< The hierarchy that fired this event.
    IObject::Ptr parent;
    IObject::Ptr child;
    IObject::Ptr old_child;
    size_t index{};

    friend bool operator==(const HierarchyChange& a, const HierarchyChange& b)
    {
        return a.type == b.type && a.hierarchy.lock() == b.hierarchy.lock() && a.parent == b.parent &&
               a.child == b.child && a.old_child == b.old_child && a.index == b.index;
    }
    friend bool operator!=(const HierarchyChange& a, const HierarchyChange& b) { return !(a == b); }
};

/**
 * @brief Manages a single-root tree of IObject references.
 *
 * Hierarchy is external to objects: objects do not know about hierarchy,
 * hierarchy knows about objects. An object can participate in multiple
 * hierarchies simultaneously.
 *
 * Chain: IInterface -> IHierarchy
 */
class IHierarchy : public Interface<IHierarchy>
{
public:
    VELK_INTERFACE(
        (EVT, on_changing),
        (EVT, on_changed)
    )

    /** @brief Sets the root object of this hierarchy. Clears existing tree. */
    virtual ReturnValue set_root(const IObject::Ptr& root) = 0;

    /** @brief Appends a child to the given parent. */
    virtual ReturnValue add(const IObject::Ptr& parent, const IObject::Ptr& child) = 0;

    /** @brief Inserts a child at the given index under the parent. */
    virtual ReturnValue insert(const IObject::Ptr& parent, size_t index, const IObject::Ptr& child) = 0;

    /** @brief Removes an object and all its descendants from the hierarchy. */
    virtual ReturnValue remove(const IObject::Ptr& object) = 0;

    /** @brief Replaces an object in-place, preserving position and reparenting children. */
    virtual ReturnValue replace(const IObject::Ptr& old_child, const IObject::Ptr& new_child) = 0;

    /** @brief Removes all objects from the hierarchy. */
    virtual void clear() = 0;

    /** @brief Returns the root object, or null if none set. */
    virtual IObject::Ptr root() const = 0;

    /** @brief Returns a snapshot of the given object's position in the hierarchy. */
    virtual HierarchyNode node_of(const IObject::Ptr& object) const = 0;

    /** @brief Returns the parent of the given object, or null if root or not in hierarchy. */
    virtual IObject::Ptr parent_of(const IObject::Ptr& object) const = 0;

    /** @brief Returns the children of the given object. */
    virtual vector<IObject::Ptr> children_of(const IObject::Ptr& object) const = 0;

    /** @brief Returns the child at the given index, or null if out of range. */
    virtual IObject::Ptr child_at(const IObject::Ptr& object, size_t index) const = 0;

    /** @brief Returns the number of children of the given object. */
    virtual size_t child_count(const IObject::Ptr& object) const = 0;

    /**
     * @brief Iterates children of the given object.
     * @param object The parent object.
     * @param context Opaque pointer forwarded to the visitor.
     * @param visitor Called for each child. Return false to stop early.
     */
    using ChildVisitorFn = bool (*)(void* context, const IObject::Ptr& child);
    virtual void for_each_child(const IObject::Ptr& object, void* context, ChildVisitorFn visitor) const = 0;

    /** @brief Returns true if the object is in this hierarchy. */
    virtual bool contains(const IObject::Ptr& object) const = 0;

    /** @brief Returns the total number of objects in this hierarchy (including root). */
    virtual size_t size() const = 0;
};

/**
 * @brief Optional interface for objects that want to be notified about hierarchy changes.
 *
 * Objects that implement this interface receive callbacks when they are added to
 * or removed from a hierarchy. The "joining" and "leaving" callbacks are pre-mutation
 * vetoes: returning false cancels the operation with ReturnValue::Refused.
 *
 * Veto rules:
 *   - on_hierarchy_joining: called for add, insert, and set_root. If the object
 *     refuses, the operation fails.
 *   - on_hierarchy_leaving: called only for direct remove (not subtree descendants).
 *     Subtree removals, clear(), and set_root() replacing an existing tree do not
 *     ask descendants for permission.
 *
 * Post-mutation notifications:
 *   - on_hierarchy_joined: called after successful add, insert, set_root, or replace
 *     (for the new object).
 *   - on_hierarchy_left: called after removal for every affected object, including
 *     subtree descendants.
 *
 * All callbacks are invoked outside the hierarchy's internal lock, so it is safe to
 * query or mutate the hierarchy from within a callback.
 *
 * Chain: IInterface -> IHierarchyAware
 */
class IHierarchyAware : public Interface<IHierarchyAware>
{
public:
    /** @brief Called before the object is added to a hierarchy. Return false to refuse. */
    virtual bool on_hierarchy_joining(const IHierarchy::Ptr& hierarchy, const IObject::Ptr& parent) = 0;

    /** @brief Called before the object is directly removed. Return false to refuse. */
    virtual bool on_hierarchy_leaving(const IHierarchy::Ptr& hierarchy) = 0;

    /** @brief Called after the object has been added to a hierarchy. */
    virtual void on_hierarchy_joined(const IHierarchy::Ptr& hierarchy, const IObject::Ptr& parent) = 0;

    /** @brief Called after the object has been removed from a hierarchy. */
    virtual void on_hierarchy_left(const IHierarchy::Ptr& hierarchy) = 0;
};

} // namespace velk

#endif // VELK_INTF_HIERARCHY_H

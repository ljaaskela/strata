#include "hierarchy.h"

#include <velk/ext/any.h>

namespace velk {

void HierarchyImpl::fire_event(string_view name, HierarchyChange change)
{
    auto evt = get_event(name, Resolve::Existing);
    if (evt && evt->has_handlers()) {
        change.hierarchy = get_self<IHierarchy>();
        auto any = ext::create_any_ref(&change);
        const IAny* arg = any.get();
        FnArgs args{&arg, 1};
        evt->invoke(args);
    }
}

void HierarchyImpl::collect_all(std::vector<IObject::Ptr>& out) const
{
    out.reserve(entries_.size());
    for (auto& [_, entry] : entries_) {
        out.push_back(entry.object);
    }
}

IObject::Ptr HierarchyImpl::lookup_parent(IObject* obj) const
{
    auto it = entries_.find(obj);
    if (it != entries_.end() && it->second.parent) {
        auto pit = entries_.find(it->second.parent);
        if (pit != entries_.end()) {
            return pit->second.object;
        }
    }
    return {};
}

ReturnValue HierarchyImpl::set_root(const IObject::Ptr& root)
{
    if (!root) {
        return ReturnValue::InvalidArgument;
    }

    auto self = get_self<IHierarchy>();

    if (auto* listener = interface_cast<IHierarchyAware>(root)) {
        if (!listener->on_hierarchy_joining(self, {})) {
            return ReturnValue::Refused;
        }
    }

    fire_event("on_changing", {HierarchyChange::Type::SetRoot, {}, {}, root});

    std::vector<IObject::Ptr> removed;
    {
        std::lock_guard lock(mutex_);
        collect_all(removed);
        root_ = root;
        entries_.clear();
        entries_[root.get()] = {root, nullptr, {}};
    }

    notify_left(removed);

    if (auto* listener = interface_cast<IHierarchyAware>(root)) {
        listener->on_hierarchy_joined(self, {});
    }

    fire_event("on_changed", {HierarchyChange::Type::SetRoot, {}, {}, root});
    return ReturnValue::Success;
}

ReturnValue HierarchyImpl::add(const IObject::Ptr& parent, const IObject::Ptr& child)
{
    if (!parent || !child) {
        return ReturnValue::InvalidArgument;
    }

    auto self = get_self<IHierarchy>();

    if (auto* listener = interface_cast<IHierarchyAware>(child)) {
        if (!listener->on_hierarchy_joining(self, parent)) {
            return ReturnValue::Refused;
        }
    }

    fire_event("on_changing", {HierarchyChange::Type::Add, {}, parent, child});

    {
        std::lock_guard lock(mutex_);
        auto pit = entries_.find(parent.get());
        if (pit == entries_.end()) {
            return ReturnValue::InvalidArgument;
        }
        if (entries_.find(child.get()) != entries_.end()) {
            return ReturnValue::InvalidArgument;
        }
        pit->second.children.push_back(child);
        entries_[child.get()] = {child, parent.get(), {}};
    }

    if (auto* listener = interface_cast<IHierarchyAware>(child)) {
        listener->on_hierarchy_joined(self, parent);
    }

    fire_event("on_changed", {HierarchyChange::Type::Add, {}, parent, child});
    return ReturnValue::Success;
}

ReturnValue HierarchyImpl::insert(const IObject::Ptr& parent, size_t index, const IObject::Ptr& child)
{
    if (!parent || !child) {
        return ReturnValue::InvalidArgument;
    }

    auto self = get_self<IHierarchy>();

    if (auto* listener = interface_cast<IHierarchyAware>(child)) {
        if (!listener->on_hierarchy_joining(self, parent)) {
            return ReturnValue::Refused;
        }
    }

    fire_event("on_changing", {HierarchyChange::Type::Insert, {}, parent, child, {}, index});

    {
        std::lock_guard lock(mutex_);
        auto pit = entries_.find(parent.get());
        if (pit == entries_.end()) {
            return ReturnValue::InvalidArgument;
        }
        if (entries_.find(child.get()) != entries_.end()) {
            return ReturnValue::InvalidArgument;
        }
        auto& children = pit->second.children;
        if (index > children.size()) {
            return ReturnValue::InvalidArgument;
        }
        children.insert(children.begin() + static_cast<ptrdiff_t>(index), child);
        entries_[child.get()] = {child, parent.get(), {}};
    }

    if (auto* listener = interface_cast<IHierarchyAware>(child)) {
        listener->on_hierarchy_joined(self, parent);
    }

    fire_event("on_changed", {HierarchyChange::Type::Insert, {}, parent, child, {}, index});
    return ReturnValue::Success;
}

ReturnValue HierarchyImpl::remove(const IObject::Ptr& object)
{
    if (!object) {
        return ReturnValue::InvalidArgument;
    }

    auto self = get_self<IHierarchy>();

    if (auto* listener = interface_cast<IHierarchyAware>(object)) {
        if (!listener->on_hierarchy_leaving(self)) {
            return ReturnValue::Refused;
        }
    }

    IObject::Ptr parent_obj;
    {
        std::shared_lock lock(mutex_);
        auto it = entries_.find(object.get());
        if (it == entries_.end()) {
            return ReturnValue::NothingToDo;
        }
        parent_obj = lookup_parent(object.get());
    }

    fire_event("on_changing", {HierarchyChange::Type::Remove, {}, parent_obj, object});

    std::vector<IObject::Ptr> removed;
    {
        std::lock_guard lock(mutex_);
        auto it = entries_.find(object.get());
        if (it == entries_.end()) {
            return ReturnValue::NothingToDo;
        }
        if (object.get() == root_.get()) {
            collect_all(removed);
            root_ = {};
            entries_.clear();
        } else {
            auto* parentPtr = it->second.parent;
            if (parentPtr) {
                auto pit = entries_.find(parentPtr);
                if (pit != entries_.end()) {
                    auto& siblings = pit->second.children;
                    for (auto sit = siblings.begin(); sit != siblings.end(); ++sit) {
                        if (sit->get() == object.get()) {
                            siblings.erase(sit);
                            break;
                        }
                    }
                }
            }
            remove_recursive(object.get(), removed);
        }
    }

    notify_left(removed);

    fire_event("on_changed", {HierarchyChange::Type::Remove, {}, parent_obj, object});
    return ReturnValue::Success;
}

ReturnValue HierarchyImpl::replace(const IObject::Ptr& old_child, const IObject::Ptr& new_child)
{
    if (!old_child || !new_child) {
        return ReturnValue::InvalidArgument;
    }

    auto self = get_self<IHierarchy>();

    IObject::Ptr parent_obj;
    {
        std::shared_lock lock(mutex_);
        auto it = entries_.find(old_child.get());
        if (it == entries_.end()) {
            return ReturnValue::InvalidArgument;
        }
        if (entries_.find(new_child.get()) != entries_.end()) {
            return ReturnValue::InvalidArgument;
        }
        parent_obj = lookup_parent(old_child.get());
    }

    if (auto* listener = interface_cast<IHierarchyAware>(new_child)) {
        if (!listener->on_hierarchy_joining(self, parent_obj)) {
            return ReturnValue::Refused;
        }
    }

    fire_event("on_changing", {HierarchyChange::Type::Replace, {}, parent_obj, new_child, old_child});

    {
        std::lock_guard lock(mutex_);
        auto it = entries_.find(old_child.get());
        if (it == entries_.end()) {
            return ReturnValue::InvalidArgument;
        }
        if (entries_.find(new_child.get()) != entries_.end()) {
            return ReturnValue::InvalidArgument;
        }
        auto* parentPtr = it->second.parent;
        auto old_children = std::move(it->second.children);

        entries_.erase(it);

        if (parentPtr) {
            auto pit = entries_.find(parentPtr);
            if (pit != entries_.end()) {
                for (auto& c : pit->second.children) {
                    if (c.get() == old_child.get()) {
                        c = new_child;
                        break;
                    }
                }
            }
        } else {
            root_ = new_child;
        }

        entries_[new_child.get()] = {new_child, parentPtr, std::move(old_children)};

        auto& newEntry = entries_[new_child.get()];
        for (auto& c : newEntry.children) {
            auto cit = entries_.find(c.get());
            if (cit != entries_.end()) {
                cit->second.parent = new_child.get();
            }
        }
    }

    if (auto* listener = interface_cast<IHierarchyAware>(old_child)) {
        listener->on_hierarchy_left(self);
    }

    if (auto* listener = interface_cast<IHierarchyAware>(new_child)) {
        listener->on_hierarchy_joined(self, parent_obj);
    }

    fire_event("on_changed", {HierarchyChange::Type::Replace, {}, parent_obj, new_child, old_child});
    return ReturnValue::Success;
}

void HierarchyImpl::clear()
{
    fire_event("on_changing", {HierarchyChange::Type::Clear});

    std::vector<IObject::Ptr> removed;
    {
        std::lock_guard lock(mutex_);
        collect_all(removed);
        root_ = {};
        entries_.clear();
    }

    notify_left(removed);

    fire_event("on_changed", {HierarchyChange::Type::Clear});
}

IObject::Ptr HierarchyImpl::root() const
{
    std::shared_lock lock(mutex_);
    return root_;
}

HierarchyNode HierarchyImpl::node_of(const IObject::Ptr& object) const
{
    if (!object) {
        return {};
    }
    std::shared_lock lock(mutex_);
    auto it = entries_.find(object.get());
    if (it == entries_.end()) {
        return {};
    }
    return {it->second.object, get_self<IHierarchy>()};
}

IObject::Ptr HierarchyImpl::parent_of(const IObject::Ptr& object) const
{
    if (!object) {
        return {};
    }
    std::shared_lock lock(mutex_);
    return lookup_parent(object.get());
}

vector<IObject::Ptr> HierarchyImpl::children_of(const IObject::Ptr& object) const
{
    if (!object) {
        return {};
    }
    std::shared_lock lock(mutex_);
    auto it = entries_.find(object.get());
    if (it == entries_.end()) {
        return {};
    }
    vector<IObject::Ptr> result;
    result.reserve(it->second.children.size());
    for (auto& c : it->second.children) {
        result.push_back(c);
    }
    return result;
}

IObject::Ptr HierarchyImpl::child_at(const IObject::Ptr& object, size_t index) const
{
    if (!object) {
        return {};
    }
    std::shared_lock lock(mutex_);
    auto it = entries_.find(object.get());
    if (it == entries_.end() || index >= it->second.children.size()) {
        return {};
    }
    return it->second.children[index];
}

size_t HierarchyImpl::child_count(const IObject::Ptr& object) const
{
    if (!object) {
        return 0;
    }
    std::shared_lock lock(mutex_);
    auto it = entries_.find(object.get());
    return it != entries_.end() ? it->second.children.size() : 0;
}

void HierarchyImpl::for_each_child(const IObject::Ptr& object, void* context, ChildVisitorFn visitor) const
{
    if (!object || !visitor) {
        return;
    }
    // Snapshot children under shared lock, then iterate outside the lock
    // so the visitor can safely mutate the hierarchy.
    std::vector<IObject::Ptr> snapshot;
    {
        std::shared_lock lock(mutex_);
        auto it = entries_.find(object.get());
        if (it == entries_.end()) {
            return;
        }
        snapshot.assign(it->second.children.begin(), it->second.children.end());
    }
    for (auto& child : snapshot) {
        if (!visitor(context, child)) {
            return;
        }
    }
}

bool HierarchyImpl::contains(const IObject::Ptr& object) const
{
    if (!object) {
        return false;
    }
    std::shared_lock lock(mutex_);
    return entries_.find(object.get()) != entries_.end();
}

size_t HierarchyImpl::size() const
{
    std::shared_lock lock(mutex_);
    return entries_.size();
}

void HierarchyImpl::remove_recursive(IObject* obj, std::vector<IObject::Ptr>& removed)
{
    auto it = entries_.find(obj);
    if (it == entries_.end()) {
        return;
    }
    auto children = std::move(it->second.children);
    removed.push_back(it->second.object);
    entries_.erase(it);
    for (auto& c : children) {
        remove_recursive(c.get(), removed);
    }
}

void HierarchyImpl::notify_left(const std::vector<IObject::Ptr>& removed)
{
    auto self = get_self<IHierarchy>();
    for (auto& obj : removed) {
        if (auto* listener = interface_cast<IHierarchyAware>(obj)) {
            listener->on_hierarchy_left(self);
        }
    }
}

} // namespace velk

#include "object_storage.h"

#include "array_property.h"
#include "event.h"
#include "function.h"
#include "property.h"

#include <velk/api/velk.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_function.h>
#include <velk/interface/types.h>

#include <algorithm>
#include <limits>

namespace velk {

static constexpr size_t AttachmentSentinel = std::numeric_limits<size_t>::max();

ObjectStorage::ObjectStorage(array_view<MemberDesc> members, IInterface* owner)
    : members_(members),
      owner_(owner),
      property_events_(members.size())
{}

array_view<MemberDesc> ObjectStorage::get_static_metadata() const
{
    return members_;
}

IInterface::Ptr ObjectStorage::create(MemberDesc desc) const
{
    // instantiate directly to avoid factory lookups
    IInterface::Ptr created;
    switch (desc.kind) {
    case MemberKind::Property: {
        {
            auto* pk = desc.propertyKind();
            created = ext::make_object<impl::Property>(pk ? pk->flags : ObjectFlags::None);
        }
        if (auto* pi = created->get_interface<IPropertyInternal>()) {
            if (auto* pk = desc.propertyKind()) {
                // Try state-backed ref first
                if (pk->createRef && owner_) {
                    if (auto* ps = interface_cast<IPropertyState>(owner_)) {
                        if (void* base = ps->get_property_state(desc.interfaceInfo->uid)) {
                            if (auto ref = pk->createRef(base)) {
                                pi->set_any(ref);
                            }
                        }
                    }
                }
                // Fallback to cloning default
                if (!pi->get_any() && pk->getDefault) {
                    if (auto* def = pk->getDefault()) {
                        if (auto any = def->clone()) {
                            pi->set_any(any);
                        }
                    }
                }
            }
        }
        break;
    }
    case MemberKind::ArrayProperty: {
        auto* pk = desc.propertyKind();
        created = ext::make_object<impl::ArrayProperty>(pk ? pk->flags : ObjectFlags::None);
        if (auto* pi = created->get_interface<IPropertyInternal>()) {
            if (pk) {
                if (pk->createRef && owner_) {
                    if (auto* ps = interface_cast<IPropertyState>(owner_)) {
                        if (void* base = ps->get_property_state(desc.interfaceInfo->uid)) {
                            if (auto ref = pk->createRef(base)) {
                                pi->set_any(ref);
                            }
                        }
                    }
                }
                if (!pi->get_any() && pk->getDefault) {
                    if (auto* def = pk->getDefault()) {
                        if (auto any = def->clone()) {
                            pi->set_any(any);
                        }
                    }
                }
            }
        }
        break;
    }
    case MemberKind::Event:
        created = ext::make_object<impl::Event>();
        break;
    case MemberKind::Function:
        created = ext::make_object<impl::Function>();
        break;
    }
    return created;
}

void ObjectStorage::bind(const MemberDesc& m, const IInterface::Ptr& fn) const
{
    auto* fk = m.functionKind();
    if (fk && fk->trampoline && owner_) {
        if (auto* fi = interface_cast<IFunctionInternal>(fn)) {
            void* intf_ptr = owner_->get_interface(m.interfaceInfo->uid);
            if (intf_ptr) {
                fi->bind(intf_ptr, fk->trampoline);
            }
        }
    }
}

IInterface::Ptr ObjectStorage::find_or_create(string_view name, MemberKind kind, Resolve mode) const
{
    auto matches = [&](const MemberDesc& m) { return m.kind == kind && m.name == name; };

    // Check cache first (skip attachment region)
    for (size_t i = attachment_end_; i < instances_.size(); ++i) {
        if (matches(members_[instances_[i].first])) {
            return instances_[i].second;
        }
    }
    if (mode == Resolve::Existing) {
        return {};
    }
    IInterface::Ptr created;
    // Find in static metadata and create if found
    for (size_t i = 0; i < members_.size(); ++i) {
        auto& m = members_[i];
        if (matches(m)) {
            // Create
            created = create(m);
            // Bind (if function)
            bind(m, created);
            // Set owner back-pointer for IStorageOwned
            if (auto* owned = interface_cast<IStorageOwned>(created)) {
                owned->set_owner(const_cast<ObjectStorage*>(this), i);
            }
            // Add to metadata
            instances_.emplace_back(i, created);
        }
    }
    return created;
}

IProperty::Ptr ObjectStorage::get_property(string_view name, Resolve mode) const
{
    auto result = find_or_create(name, MemberKind::Property, mode);
    if (!result) {
        result = find_or_create(name, MemberKind::ArrayProperty, mode);
    }
    return interface_pointer_cast<IProperty>(result);
}

IEvent::Ptr ObjectStorage::get_event(string_view name, Resolve mode) const
{
    return interface_pointer_cast<IEvent>(find_or_create(name, MemberKind::Event, mode));
}

IFunction::Ptr ObjectStorage::get_function(string_view name, Resolve mode) const
{
    return interface_pointer_cast<IFunction>(find_or_create(name, MemberKind::Function, mode));
}

void ObjectStorage::notify(MemberKind kind, Uid interfaceUid, Notification notification) const
{
    // Iterate only metadata entries (skip attachment region)
    for (size_t i = attachment_end_; i < instances_.size(); ++i) {
        auto idx = instances_[i].first;
        auto& m = members_[idx];
        if (m.kind != kind || !m.interfaceInfo || m.interfaceInfo->uid != interfaceUid) {
            continue;
        }

        switch (notification) {
        case Notification::Changed:
            if (kind == MemberKind::Property || kind == MemberKind::ArrayProperty) {
                invoke_property_changed(idx, interface_cast<IProperty>(instances_[i].second));
            }
            break;
        default:
            break;
        }
    }
}

ReturnValue ObjectStorage::add_attachment(const IInterface::Ptr& attachment)
{
    if (!attachment) {
        return ReturnValue::InvalidArgument;
    }
    instances_.insert(instances_.begin() + attachment_end_, {AttachmentSentinel, attachment});
    attachment_end_++;
    return ReturnValue::Success;
}

ReturnValue ObjectStorage::remove_attachment(const IInterface::Ptr& attachment)
{
    if (!attachment) {
        return ReturnValue::InvalidArgument;
    }
    for (uint32_t i = 0; i < attachment_end_; ++i) {
        if (instances_[i].second.get() == attachment.get()) {
            instances_.erase(instances_.begin() + i);
            attachment_end_--;
            return ReturnValue::Success;
        }
    }
    return ReturnValue::NothingToDo;
}

size_t ObjectStorage::attachment_count() const
{
    return attachment_end_;
}

IInterface::Ptr ObjectStorage::get_attachment(size_t index) const
{
    if (index < attachment_end_) {
        return instances_[index].second;
    }
    return {};
}

IInterface::Ptr ObjectStorage::find_attachment(const AttachmentQuery& query, Resolve mode)
{
    const bool matchInterface = query.interfaceUid != Uid{};
    const bool matchClass = query.classUid != Uid{};

    for (uint32_t i = 0; i < attachment_end_; ++i) {
        auto& att = instances_[i].second;
        if (matchInterface && !att->get_interface(query.interfaceUid)) {
            continue;
        }
        if (matchClass) {
            if (auto* obj = att->get_interface<IObject>()) {
                if (obj->get_class_uid() != query.classUid) {
                    continue;
                }
            }
        }
        return att;
    }
    if (mode == Resolve::Create && matchClass) {
        auto created = instance().create<IInterface>(query.classUid);
        if (created) {
            add_attachment(created);
        }
        return created;
    }
    return {};
}

IEvent::Ptr ObjectStorage::get_property_event(size_t storage_id, Resolve mode) const
{
    if (storage_id >= property_events_.size()) {
        return {};
    }
    if (!property_events_[storage_id] && mode == Resolve::Create) {
        instance().type_registry().create_event_once(property_events_[storage_id]);
    }
    return property_events_[storage_id];
}

void ObjectStorage::invoke_property_changed(size_t storage_id, IProperty* property) const
{
    if (property) {
        if (storage_id < property_events_.size()) {
            invoke_event(property_events_[storage_id], property->get_value().get());
        }
        for (auto* observer : observers_) {
            observer->on_property_changed(*property);
        }
    }
}

void ObjectStorage::invoke_property_changed(size_t storage_id) const
{
    // Find the property instance by storage_id (member index)
    IProperty* prop = nullptr;
    for (size_t i = attachment_end_; i < instances_.size(); ++i) {
        if (instances_[i].first == storage_id) {
            prop = interface_cast<IProperty>(instances_[i].second);
            break;
        }
    }
    invoke_property_changed(storage_id, prop);
}

void ObjectStorage::add_observer(IMetadataObserver* observer)
{
    if (observer) {
        observers_.push_back(observer);
    }
}

void ObjectStorage::remove_observer(IMetadataObserver* observer)
{
    auto it = std::find(observers_.begin(), observers_.end(), observer);
    if (it != observers_.end()) {
        observers_.erase(it);
    }
}

} // namespace velk

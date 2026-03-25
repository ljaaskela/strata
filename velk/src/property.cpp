#include "property.h"

#include "dependency_tracker.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_any_extension.h>
#include <velk/interface/intf_external_any.h>
#include <velk/interface/types.h>

namespace velk::impl {

Property::~Property()
{
    // Storage-owned: data lives in MemberSlot, cleaned up by ObjectStorage::destroy_slot().
    // Standalone: clean up the extension chain and delete the heap-allocated slot.
    if (has_standalone_slot()) {
        auto& s = slot();
        auto current = std::move(s.data.property.data);
        while (auto* ext = interface_cast<IAnyExtension>(current)) {
            current = ext->take_inner(*static_cast<IInterface*>(static_cast<void*>(this)));
        }
        s.data.property.data.~shared_ptr();
        s.data.property.event.~shared_ptr();
        delete_standalone_slot();
    }
}

ReturnValue Property::set_value(const IAny& from, InvokeType type)
{
    type = resolve_invoke_type(type, get_object_data().owner_thread_id);
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    auto& any = slot_any();
    if (!any) {
        return ReturnValue::Fail;
    }
    if (type == Deferred) {
        // Create a clone with value "from" and store it in the deferred callback
        auto clone = any->clone();
        if (clone && clone->copy_from(from) == ReturnValue::Success) {
            instance().queue_deferred_property({get_self<IPropertyInternal>(), std::move(clone)});
            return ReturnValue::Success;
        }
        return ReturnValue::Fail;
    }
    // type == Immediate, just copy the value
    auto ret = any->copy_from(from);
    if (ret == ReturnValue::Success && !external_) {
        invoke_on_changed();
    }
    return ret;
}
const IAny::ConstPtr Property::get_value() const
{
    detail::record_dependency(this);
    return slot_any();
}

bool Property::set_any(const IAny::Ptr& value, IAny::Ptr* previous)
{
    if (previous) {
        *previous = {};
    }
    auto& any = slot_any();
    if (any && value) {
        // Disconnect old external wiring if present
        if (external_) {
            if (auto ext = interface_cast<IExternalAny>(any)) {
                ext->on_data_changed()->remove_handler(on_changed());
            }
        }
        if (previous) {
            *previous = any;
        }
    }
    any = value;
    auto external = interface_cast<IExternalAny>(any);
    external_ = external != nullptr;
    if (external) {
        // External any fires on_data_changed when its data changes, so connect it to
        // our on_changed. PropertyImpl skips its own explicit fire for external anys.
        external->on_data_changed()->add_handler(on_changed());
    }
    invoke_on_changed();
    return true;
}
IAny::ConstPtr Property::get_any() const
{
    detail::record_dependency(this);
    return slot_any();
}
ReturnValue Property::set_value_silent(const IAny& from)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    auto& any = slot_any();
    if (!any) {
        return ReturnValue::Fail;
    }
    auto ret = any->copy_from(from);
    // External anys fire on_data_changed -> on_changed automatically during copy_from,
    // so return NothingToDo to prevent the caller from firing on_changed again.
    if (ret == ReturnValue::Success && external_) {
        return ReturnValue::NothingToDo;
    }
    return ret;
}

ReturnValue Property::set_data(const void* data, size_t size, Uid type, InvokeType invokeType)
{
    invokeType = resolve_invoke_type(invokeType, get_object_data().owner_thread_id);
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    auto& any = slot_any();
    if (!any) {
        return ReturnValue::Fail;
    }
    if (invokeType == Deferred) {
        auto clone = any->clone();
        if (clone && clone->set_data(data, size, type) == ReturnValue::Success) {
            instance().queue_deferred_property({get_self<IPropertyInternal>(), std::move(clone)});
            return ReturnValue::Success;
        }
        return ReturnValue::Fail;
    }
    auto ret = any->set_data(data, size, type);
    if (ret == ReturnValue::Success && !external_) {
        invoke_on_changed();
    }
    return ret;
}

bool Property::install_extension(const IAnyExtension::Ptr& extension)
{
    if (!extension) {
        return false;
    }
    if (!extension->set_inner(slot_any(), IInterface::WeakPtr(get_self<IInterface>()))) {
        return false;
    }
    set_any(interface_pointer_cast<IAny>(extension));
    return true;
}

bool Property::remove_extension(const IAnyExtension::Ptr& extension)
{
    if (!extension) {
        return false;
    }
    auto& any = slot_any();
    if (!any) {
        return false;
    }

    auto ext_as_any = interface_pointer_cast<IAny>(extension);

    // Use get_interface to get a consistent IInterface pointer that matches
    // the one stored by install_extension (via get_self<IInterface>()).
    // A direct static_cast would go through the IPropertyInternal chain,
    // which yields a different IInterface sub-object than get_interface returns.
    IInterface& self = *get_interface<IInterface>();

    // Case 1: extension is at the head of the chain
    if (any == ext_as_any) {
        auto inner = extension->take_inner(self);
        set_any(inner);
        return true;
    }

    // Case 2: extension is deeper in the chain; walk to find its predecessor.
    // Use take/restore to get non-const access to each link.
    auto weakSelf = IInterface::WeakPtr(get_self<IInterface>());
    auto* prev = interface_cast<IAnyExtension>(any);
    while (prev) {
        auto inner = prev->take_inner(self);
        if (inner == ext_as_any) {
            prev->set_inner(extension->take_inner(self), weakSelf);
            return true;
        }
        auto* next = interface_cast<IAnyExtension>(inner);
        prev->set_inner(std::move(inner), weakSelf);
        prev = next;
    }
    return false;
}

void Property::invoke_on_changed() const
{
    if (is_standalone()) {
        return;
    }
    auto* self = const_cast<IProperty*>(static_cast<const IProperty*>(this));
    ::velk::invoke_property_changed(get_owner(), get_storage_id(), self);
}

} // namespace velk::impl

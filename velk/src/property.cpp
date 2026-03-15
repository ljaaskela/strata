#include "property.h"

#include "dependency_tracker.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_any_extension.h>
#include <velk/interface/intf_external_any.h>
#include <velk/interface/types.h>

namespace velk::impl {

ReturnValue Property::set_value(const IAny& from, InvokeType type)
{
    type = resolve_invoke_type(type, get_object_data().owner_thread_id);
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    if (!data_) {
        return ReturnValue::Fail;
    }
    if (type == Deferred) {
        // Create a clone with value "from" and store it in the deferred callback
        auto clone = data_->clone();
        if (clone && clone->copy_from(from) == ReturnValue::Success) {
            instance().queue_deferred_property({get_self<IPropertyInternal>(), std::move(clone)});
            return ReturnValue::Success;
        }
        return ReturnValue::Fail;
    }
    // type == Immediate, just copy the value
    auto ret = data_->copy_from(from);
    if (ret == ReturnValue::Success && !external_) {
        invoke_event(on_changed(), data_.get());
    }
    return ret;
}
const IAny::ConstPtr Property::get_value() const
{
    detail::record_dependency(this);
    return data_;
}

bool Property::set_any(const IAny::Ptr& value, IAny::Ptr* previous)
{
    if (previous) {
        *previous = {};
    }
    if (data_ && value) {
        // Disconnect old external wiring if present
        if (external_) {
            if (auto ext = interface_cast<IExternalAny>(data_)) {
                ext->on_data_changed()->remove_handler(on_changed());
            }
        }
        if (previous) {
            *previous = data_;
        }
    }
    data_ = value;
    auto external = interface_cast<IExternalAny>(data_);
    external_ = external != nullptr;
    if (external) {
        // External any fires on_data_changed when its data changes, so connect it to
        // our on_changed. PropertyImpl skips its own explicit fire for external anys.
        external->on_data_changed()->add_handler(on_changed());
    }
    return succeeded(invoke_event(on_changed(), data_.get()));
}
IAny::ConstPtr Property::get_any() const
{
    detail::record_dependency(this);
    return data_;
}
ReturnValue Property::set_value_silent(const IAny& from)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    if (!data_) {
        return ReturnValue::Fail;
    }
    auto ret = data_->copy_from(from);
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
    if (!data_) {
        return ReturnValue::Fail;
    }
    if (invokeType == Deferred) {
        auto clone = data_->clone();
        if (clone && clone->set_data(data, size, type) == ReturnValue::Success) {
            instance().queue_deferred_property({get_self<IPropertyInternal>(), std::move(clone)});
            return ReturnValue::Success;
        }
        return ReturnValue::Fail;
    }
    auto ret = data_->set_data(data, size, type);
    if (ret == ReturnValue::Success && !external_) {
        invoke_event(on_changed(), data_.get());
    }
    return ret;
}

bool Property::install_extension(const IAnyExtension::Ptr& extension)
{
    if (!extension) {
        return false;
    }
    if (!extension->set_inner(data_, IInterface::WeakPtr(get_self<IInterface>()))) {
        return false;
    }
    set_any(interface_pointer_cast<IAny>(extension));
    return true;
}

bool Property::remove_extension(const IAnyExtension::Ptr& extension)
{
    if (!extension || !data_) {
        return false;
    }

    auto ext_as_any = interface_pointer_cast<IAny>(extension);

    // Use get_interface to get a consistent IInterface pointer that matches
    // the one stored by install_extension (via get_self<IInterface>()).
    // A direct static_cast would go through the IPropertyInternal chain,
    // which yields a different IInterface sub-object than get_interface returns.
    IInterface& self = *get_interface<IInterface>();

    // Case 1: extension is at the head of the chain
    if (data_ == ext_as_any) {
        auto inner = extension->take_inner(self);
        set_any(inner);
        return true;
    }

    // Case 2: extension is deeper in the chain; walk to find its predecessor.
    // Use take/restore to get non-const access to each link.
    auto weakSelf = IInterface::WeakPtr(get_self<IInterface>());
    auto* prev = interface_cast<IAnyExtension>(data_);
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

} // namespace velk::impl

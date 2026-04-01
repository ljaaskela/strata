#include "standalone_property.h"

#include "dependency_tracker.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_any_extension.h>
#include <velk/interface/intf_external_any.h>
#include <velk/interface/types.h>

namespace velk::impl {

StandaloneProperty::~StandaloneProperty()
{
    // Walk the extension chain before releasing data_, so extensions can clean up.
    auto current = std::move(data_);
    while (auto* ext = interface_cast<IAnyExtension>(current)) {
        current = ext->take_inner(*static_cast<IInterface*>(static_cast<void*>(this)));
    }
}

ReturnValue StandaloneProperty::set_value(const IAny& from, InvokeType type)
{
    type = resolve_invoke_type(type, get_object_data().owner_thread_id);
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    if (!data_) {
        return ReturnValue::Fail;
    }
    if (type == Deferred) {
        auto clone = data_->clone();
        if (clone && clone->copy_from(from) == ReturnValue::Success) {
            instance().queue_deferred_property({get_self<IPropertyInternal>(), std::move(clone)});
            return ReturnValue::Success;
        }
        return ReturnValue::Fail;
    }
    auto ret = data_->copy_from(from);
    if (ret == ReturnValue::Success && !external_) {
        invoke_on_changed();
    }
    return ret;
}

const IAny::ConstPtr StandaloneProperty::get_value() const
{
    detail::record_dependency(this);
    return data_;
}

IEvent::Ptr StandaloneProperty::on_changed() const
{
    if (!event_) {
        instance().type_registry().create_event_once(event_);
    }
    return event_;
}

bool StandaloneProperty::set_any(const IAny::Ptr& value, IAny::Ptr* previous)
{
    if (previous) {
        *previous = {};
    }
    if (data_ && value) {
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
        external->on_data_changed()->add_handler(on_changed());
    }
    invoke_on_changed();
    return true;
}

IAny::ConstPtr StandaloneProperty::get_any() const
{
    detail::record_dependency(this);
    return data_;
}

ReturnValue StandaloneProperty::set_value_silent(const IAny& from)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    if (!data_) {
        return ReturnValue::Fail;
    }
    auto ret = data_->copy_from(from);
    if (ret == ReturnValue::Success && external_) {
        return ReturnValue::NothingToDo;
    }
    return ret;
}

ReturnValue StandaloneProperty::set_data(const void* data, size_t size, Uid type,
                                         InvokeType invokeType)
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
        invoke_on_changed();
    }
    return ret;
}

bool StandaloneProperty::install_extension(const IAnyExtension::Ptr& extension)
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

bool StandaloneProperty::remove_extension(const IAnyExtension::Ptr& extension)
{
    if (!extension) {
        return false;
    }
    if (!data_) {
        return false;
    }

    auto ext_as_any = interface_pointer_cast<IAny>(extension);
    IInterface& self = *get_interface<IInterface>();

    // Case 1: extension is at the head of the chain
    if (data_ == ext_as_any) {
        auto inner = extension->take_inner(self);
        set_any(inner);
        return true;
    }

    // Case 2: extension is deeper in the chain
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

void StandaloneProperty::invoke_on_changed() const
{
    if (event_) {
        auto* self = const_cast<IProperty*>(static_cast<const IProperty*>(this));
        invoke_event(event_, self->get_value().get());
    }
    if (notify_fn_) {
        auto* self = const_cast<IProperty*>(static_cast<const IProperty*>(this));
        notify_fn_(notify_ctx_, name_, self);
    }
}

} // namespace velk::impl

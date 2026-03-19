#include "array_property.h"

#include <velk/api/velk.h>
#include <velk/interface/types.h>

namespace velk::impl {

// IProperty

ReturnValue ArrayProperty::set_value(const IAny& from, InvokeType type)
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
    if (ret == ReturnValue::Success) {
        invoke_on_changed();
    }
    return ret;
}

const IAny::ConstPtr ArrayProperty::get_value() const
{
    return data_;
}

// IPropertyInternal

bool ArrayProperty::set_any(const IAny::Ptr& value, IAny::Ptr* previous)
{
    if (previous) {
        *previous = {};
    }
    if (data_ && value) {
        if (previous) {
            *previous = data_;
        }
    }
    data_ = value;
    invoke_on_changed();
    return true;
}

IAny::ConstPtr ArrayProperty::get_any() const
{
    return data_;
}

ReturnValue ArrayProperty::set_data(const void* data, size_t size, Uid type, InvokeType invokeType)
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
    if (ret == ReturnValue::Success) {
        invoke_on_changed();
    }
    return ret;
}

ReturnValue ArrayProperty::set_value_silent(const IAny& from)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    if (!data_) {
        return ReturnValue::Fail;
    }
    return data_->copy_from(from);
}

bool ArrayProperty::install_extension(const IAnyExtension::Ptr& extension)
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

bool ArrayProperty::remove_extension(const IAnyExtension::Ptr& extension)
{
    if (!extension || !data_) {
        return false;
    }

    auto ext_as_any = interface_pointer_cast<IAny>(extension);

    IInterface& self = *get_interface<IInterface>();

    if (data_ == ext_as_any) {
        auto inner = extension->take_inner(self);
        set_any(inner);
        return true;
    }

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

// IArrayProperty (delegates to IArrayAny on data_)

IArrayAny* ArrayProperty::get_array_any() const
{
    return data_ ? interface_cast<IArrayAny>(data_) : nullptr;
}

size_t ArrayProperty::array_size() const
{
    auto* aa = get_array_any();
    return aa ? aa->array_size() : 0;
}

ReturnValue ArrayProperty::get_at(size_t index, IAny& out) const
{
    auto* aa = get_array_any();
    return aa ? aa->get_at(index, out) : ReturnValue::Fail;
}

ReturnValue ArrayProperty::set_at(size_t index, const IAny& value)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    auto* aa = get_array_any();
    if (!aa) {
        return ReturnValue::Fail;
    }
    auto ret = aa->set_at(index, value);
    if (succeeded(ret)) {
        invoke_on_changed();
    }
    return ret;
}

ReturnValue ArrayProperty::push_back(const IAny& value)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    auto* aa = get_array_any();
    if (!aa) {
        return ReturnValue::Fail;
    }
    auto ret = aa->push_back(value);
    if (succeeded(ret)) {
        invoke_on_changed();
    }
    return ret;
}

ReturnValue ArrayProperty::erase_at(size_t index)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    auto* aa = get_array_any();
    if (!aa) {
        return ReturnValue::Fail;
    }
    auto ret = aa->erase_at(index);
    if (succeeded(ret)) {
        invoke_on_changed();
    }
    return ret;
}

void ArrayProperty::clear_array()
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return;
    }
    auto* aa = get_array_any();
    if (!aa) {
        return;
    }
    aa->clear_array();
    invoke_on_changed();
}

void ArrayProperty::invoke_on_changed() const
{
    auto* self = const_cast<ArrayProperty*>(this)->get_interface<IProperty>();
    ::velk::invoke_property_changed(owner_, storage_id_, self);
}

} // namespace velk::impl

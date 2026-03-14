#include "object_ref.h"

#include <velk/interface/intf_object_ref.h>

namespace velk {

namespace {

Uid object_ptr_type()
{
    return type_uid<IObject::Ptr>();
}

} // namespace

// IAny

array_view<Uid> ObjectRefImpl::get_compatible_types() const
{
    static const Uid types[] = {object_ptr_type()};
    return {types, 1};
}

size_t ObjectRefImpl::get_data_size(Uid type) const
{
    return type == object_ptr_type() ? sizeof(IObject::Ptr) : 0;
}

ReturnValue ObjectRefImpl::get_data(void* to, size_t toSize, Uid type) const
{
    if (!to || type != object_ptr_type() || toSize < sizeof(IObject::Ptr)) {
        return ReturnValue::Fail;
    }
    IObject::Ptr result;
    if (owning_) {
        result = strong_;
    } else {
        result = weak_.lock();
    }
    new (to) IObject::Ptr(std::move(result));
    return ReturnValue::Success;
}

ReturnValue ObjectRefImpl::set_data(const void* from, size_t fromSize, Uid type)
{
    if (!from || type != object_ptr_type() || fromSize < sizeof(IObject::Ptr)) {
        return ReturnValue::Fail;
    }
    const auto& obj = *static_cast<const IObject::Ptr*>(from);
    return set_object(obj);
}

ReturnValue ObjectRefImpl::copy_from(const IAny& other)
{
    // If the other is also an ObjectRef, copy the object pointer directly
    if (auto* ref = interface_cast<const IObjectRef>(&other)) {
        auto obj = ref->get_object();
        if (!obj) {
            // Clear our reference
            bool had = (strong_ != nullptr || !weak_.expired());
            strong_ = nullptr;
            weak_.reset();
            return had ? ReturnValue::Success : ReturnValue::NothingToDo;
        }
        return set_object(obj);
    }

    // Try to extract IObject::Ptr from a generic IAny
    if (is_compatible(other, object_ptr_type())) {
        IObject::Ptr obj;
        if (succeeded(other.get_data(&obj, sizeof(IObject::Ptr), object_ptr_type()))) {
            return set_object(obj);
        }
    }
    return ReturnValue::Fail;
}

IAny::Ptr ObjectRefImpl::clone() const
{
    auto obj = ext::make_object<ObjectRefImpl>();
    auto* impl = static_cast<ObjectRefImpl*>(obj.get());
    impl->constraint_ = constraint_;
    impl->owning_ = owning_;
    impl->strong_ = strong_;
    impl->weak_ = weak_;
    return interface_pointer_cast<IAny>(obj);
}

// IObjectRef

IObject::Ptr ObjectRefImpl::get_object() const
{
    if (owning_) {
        return strong_;
    }
    return weak_.lock();
}

ReturnValue ObjectRefImpl::set_object(const IObject::Ptr& obj)
{
    if (obj) {
        // Validate constraint
        if (constraint_ != Uid{} && !obj->get_interface(constraint_)) {
            return ReturnValue::InvalidArgument;
        }
        // Check if same object
        if (owning_) {
            if (strong_.get() == obj.get()) {
                return ReturnValue::NothingToDo;
            }
        } else {
            auto locked = weak_.lock();
            if (locked.get() == obj.get()) {
                return ReturnValue::NothingToDo;
            }
        }
        weak_ = obj;
        if (owning_) {
            strong_ = obj;
        }
    } else {
        if (!strong_ && !weak_.lock()) {
            return ReturnValue::NothingToDo;
        }
        strong_ = nullptr;
        weak_ = {};
    }
    return ReturnValue::Success;
}

bool ObjectRefImpl::is_owning() const
{
    return owning_;
}

ReturnValue ObjectRefImpl::set_owning(bool owning)
{
    if (owning_ == owning) {
        return ReturnValue::NothingToDo;
    }
    if (owning) {
        // Switching to owning: need to lock the weak ref
        auto locked = weak_.lock();
        if (!locked) {
            return ReturnValue::Fail;
        }
        strong_ = locked;
    } else {
        // Switching to non-owning: release strong ref
        strong_ = nullptr;
    }
    owning_ = owning;
    return ReturnValue::Success;
}

Uid ObjectRefImpl::constraint_uid() const
{
    return constraint_;
}

ReturnValue ObjectRefImpl::set_constraint(Uid uid)
{
    constraint_ = uid;
    return ReturnValue::Success;
}

} // namespace velk

#include "type_registry.h"

#include "array_property.h"
#include "binding.h"
#include "event.h"
#include "function.h"
#include "future.h"
#include "hierarchy.h"
#include "hive/hive_store.h"
#include "hive/object_hive.h"
#include "hive/raw_hive.h"
#include "property.h"
#include "object_ref.h"
#include "variant.h"

#include <velk/ext/any.h>
#include <velk/interface/intf_log.h>
#include <velk/string.h>

#include <algorithm>
#include <shared_mutex>

namespace velk {

TypeRegistry::TypeRegistry(ILog& log) : log_(log)
{
    ITypeRegistry::register_type<PropertyImpl>();
    ITypeRegistry::register_type<ArrayPropertyImpl>();
    ITypeRegistry::register_type<FunctionImpl>();
    ITypeRegistry::register_type<EventImpl>();
    ITypeRegistry::register_type<FutureImpl>();
    ITypeRegistry::register_type<HiveStore>();
    ITypeRegistry::register_type<ObjectHive>();
    ITypeRegistry::register_type<RawHiveImpl>();
    ITypeRegistry::register_type<HierarchyImpl>();
    ITypeRegistry::register_type<BindingImpl>();
    ITypeRegistry::register_type<VariantImpl>();
    ITypeRegistry::register_type<ObjectRefImpl>();

    ITypeRegistry::register_type<ext::AnyValue<bool>>();
    ITypeRegistry::register_type<ext::AnyValue<float>>();
    ITypeRegistry::register_type<ext::AnyValue<double>>();
    ITypeRegistry::register_type<ext::AnyValue<uint8_t>>();
    ITypeRegistry::register_type<ext::AnyValue<uint16_t>>();
    ITypeRegistry::register_type<ext::AnyValue<uint32_t>>();
    ITypeRegistry::register_type<ext::AnyValue<uint64_t>>();
    ITypeRegistry::register_type<ext::AnyValue<int8_t>>();
    ITypeRegistry::register_type<ext::AnyValue<int16_t>>();
    ITypeRegistry::register_type<ext::AnyValue<int32_t>>();
    ITypeRegistry::register_type<ext::AnyValue<int64_t>>();
    ITypeRegistry::register_type<ext::AnyValue<string>>();
    ITypeRegistry::register_type<ext::AnyValue<Duration>>();
    ITypeRegistry::register_type<ext::AnyValue<HierarchyChange>>();

    ITypeRegistry::register_type<ext::ArrayAnyValue<float>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<double>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<uint8_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<uint16_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<uint32_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<uint64_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<int8_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<int16_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<int32_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<int64_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<string>>();
}

const IObjectFactory* TypeRegistry::find(Uid uid) const
{
    std::shared_lock lock(mutex_);
    Entry key{uid, nullptr};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == uid) {
        return it->factory;
    }
    return nullptr;
}

ReturnValue TypeRegistry::register_type(const IObjectFactory& factory)
{
    auto& info = factory.get_class_info();
    detail::velk_log(log_,
                     LogLevel::Debug,
                     __FILE__,
                     __LINE__,
                     "Register %.*s",
                     static_cast<int>(info.name.size()),
                     info.name.data());
    std::unique_lock lock(mutex_);
    Entry entry{info.uid, &factory, current_owner_};
    auto it = std::lower_bound(types_.begin(), types_.end(), entry);
    if (it != types_.end() && it->uid == info.uid) {
        it->factory = &factory;
        it->owner = current_owner_;
    } else {
        types_.insert(it, entry);
    }
    return ReturnValue::Success;
}

ReturnValue TypeRegistry::unregister_type(const IObjectFactory& factory)
{
    std::unique_lock lock(mutex_);
    Entry key{factory.get_class_info().uid, nullptr};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == key.uid) {
        types_.erase(it);
    }
    return ReturnValue::Success;
}

IInterface::Ptr TypeRegistry::create(Uid uid, uint32_t flags) const
{
    if (auto* factory = find(uid)) {
        auto object = factory->create_instance(flags);
        if (!object) {
            VELK_LOG(
                E, "Failed to instantiate %s: %s", factory->get_class_info().name, to_string(uid).c_str());
        }
        return object;
    }
    VELK_LOG(E, "Failed to instantiate %s", to_string(uid).c_str());
    return {};
}

const ClassInfo* TypeRegistry::get_class_info(Uid classUid) const
{
    if (auto* factory = find(classUid)) {
        return &factory->get_class_info();
    }
    return nullptr;
}

const IObjectFactory* TypeRegistry::find_factory(Uid classUid) const
{
    return find(classUid);
}

void TypeRegistry::set_owner(Uid uid)
{
    std::unique_lock lock(mutex_);
    current_owner_ = uid;
}

void TypeRegistry::sweep_owner(Uid uid)
{
    std::unique_lock lock(mutex_);
    types_.erase(std::remove_if(types_.begin(), types_.end(), [&](const Entry& e) { return e.owner == uid; }),
                 types_.end());
    interpolators_.erase(std::remove_if(interpolators_.begin(),
                                        interpolators_.end(),
                                        [&](const InterpolatorEntry& e) { return e.owner == uid; }),
                         interpolators_.end());
}

ReturnValue TypeRegistry::register_interpolator(Uid typeUid, InterpolatorFn fn)
{
    std::unique_lock lock(mutex_);
    InterpolatorEntry entry{typeUid, fn, current_owner_};
    auto it = std::lower_bound(interpolators_.begin(), interpolators_.end(), entry);
    if (it != interpolators_.end() && it->typeUid == typeUid) {
        it->fn = fn;
        it->owner = current_owner_;
    } else {
        interpolators_.insert(it, entry);
    }
    return ReturnValue::Success;
}

ReturnValue TypeRegistry::unregister_interpolator(Uid typeUid)
{
    std::unique_lock lock(mutex_);
    InterpolatorEntry key{typeUid, nullptr, {}};
    auto it = std::lower_bound(interpolators_.begin(), interpolators_.end(), key);
    if (it != interpolators_.end() && it->typeUid == typeUid) {
        interpolators_.erase(it);
        return ReturnValue::Success;
    }
    return ReturnValue::NothingToDo;
}

InterpolatorFn TypeRegistry::find_interpolator(Uid typeUid) const
{
    std::shared_lock lock(mutex_);
    InterpolatorEntry key{typeUid, nullptr, {}};
    auto it = std::lower_bound(interpolators_.begin(), interpolators_.end(), key);
    if (it != interpolators_.end() && it->typeUid == typeUid) {
        return it->fn;
    }
    return nullptr;
}

IAny::Ptr TypeRegistry::create_any(Uid type) const
{
    return interface_pointer_cast<IAny>(create(type));
}

IVariant::Ptr TypeRegistry::create_variant() const
{
    static const auto& factory = VariantImpl::get_factory();
    return interface_pointer_cast<IVariant>(factory.create_instance());
}

IObjectRef::Ptr TypeRegistry::create_object_ref() const
{
    static const auto& factory = ObjectRefImpl::get_factory();
    return interface_pointer_cast<IObjectRef>(factory.create_instance());
}

IFuture::Ptr TypeRegistry::create_future() const
{
    static const auto& factory = FutureImpl::get_factory();
    return interface_pointer_cast<IFuture>(factory.create_instance());
}

IFunction::Ptr TypeRegistry::create_callback(IFunction::CallableFn* fn) const
{
    static const auto& factory = FunctionImpl::get_factory();
    auto func = interface_pointer_cast<IFunction>(factory.create_instance());
    if (fn) {
        if (auto* internal = interface_cast<IFunctionInternal>(func)) {
            internal->set_invoke_callback(fn);
        }
    }
    return func;
}

IFunction::Ptr TypeRegistry::create_owned_callback(void* context, IFunction::BoundFn* fn,
                                                   IFunction::ContextDeleter* deleter) const
{
    static const auto& factory = FunctionImpl::get_factory();
    auto func = interface_pointer_cast<IFunction>(factory.create_instance());
    if (fn && deleter) {
        if (auto* internal = interface_cast<IFunctionInternal>(func)) {
            internal->set_owned_callback(context, fn, deleter);
        }
    }
    return func;
}

IProperty::Ptr TypeRegistry::create_property(Uid type, const IAny::Ptr& value, uint32_t flags) const
{
    static const auto& factory = PropertyImpl::get_factory();
    auto property = interface_pointer_cast<IProperty>(factory.create_instance(flags));
    if (auto pi = interface_cast<IPropertyInternal>(property)) {
        if (value && is_compatible(value, type)) {
            if (pi->set_any(value)) {
                return property;
            }
            VELK_LOG(E, "Initial value is of incompatible type");
        }
        // Any was not specified for property instance, create new one
        if (auto any = create_any(type)) {
            if (pi->set_any(any)) {
                return property;
            }
        }
    }
    return {};
}

} // namespace velk

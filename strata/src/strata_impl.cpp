#include "metadata_container.h"
#include "strata_impl.h"
#include "event.h"
#include "function.h"
#include "property.h"
#include <ext/any.h>
#include <interface/types.h>
#include <iostream>

namespace strata {

void RegisterTypes(IStrata &strata)
{
    strata.RegisterType<PropertyImpl>();
    strata.RegisterType<EventImpl>();
    strata.RegisterType<FunctionImpl>();

    strata.RegisterType<SimpleAny<float>>();
    strata.RegisterType<SimpleAny<double>>();
    strata.RegisterType<SimpleAny<uint8_t>>();
    strata.RegisterType<SimpleAny<uint16_t>>();
    strata.RegisterType<SimpleAny<uint32_t>>();
    strata.RegisterType<SimpleAny<uint64_t>>();
    strata.RegisterType<SimpleAny<int8_t>>();
    strata.RegisterType<SimpleAny<int16_t>>();
    strata.RegisterType<SimpleAny<int32_t>>();
    strata.RegisterType<SimpleAny<int64_t>>();
    strata.RegisterType<SimpleAny<std::string>>();
}

StrataImpl::StrataImpl()
{
    RegisterTypes(*this);
}

ReturnValue StrataImpl::RegisterType(const IObjectFactory &factory)
{
    auto &info = factory.GetClassInfo();
    std::cout << "Register " << info.name << " (uid: " << info.uid << ")" << std::endl;
    types_[info.uid] = &factory;
    return ReturnValue::SUCCESS;
}

ReturnValue StrataImpl::UnregisterType(const IObjectFactory &factory)
{
    types_.erase(factory.GetClassInfo().uid);
    return ReturnValue::SUCCESS;
}

IInterface::Ptr StrataImpl::Create(Uid uid) const
{
    if (auto fac = types_.find(uid); fac != types_.end()) {
        if (auto object = fac->second->CreateInstance()) {
            if (auto shared = object->GetInterface<ISharedFromObject>()) {
                shared->SetSelf(object);
            }
            auto& info = fac->second->GetClassInfo();
            if (!info.members.empty()) {
                if (auto *meta = interface_cast<IMetadataContainer>(object)) {
                    // Object takes ownership
                    meta->SetMetadataContainer(new MetadataContainer(info.members, *this));
                }
            }
            return object;
        }
    }
    return {};
}

const ClassInfo* StrataImpl::GetClassInfo(Uid classUid) const
{
    if (auto fac = types_.find(classUid); fac != types_.end()) {
        return &fac->second->GetClassInfo();
    }
    return nullptr;
}

IAny::Ptr StrataImpl::CreateAny(Uid type) const
{
    return interface_pointer_cast<IAny>(Create(type));
}

IProperty::Ptr StrataImpl::CreateProperty(Uid type, const IAny::Ptr &value) const
{
    if (auto property = interface_pointer_cast<IProperty>(Create(ClassId::Property))) {
        if (auto pi = property->GetInterface<IPropertyInternal>()) {
            if (value && IsCompatible(value, type)) {
                if (pi->SetAny(value)) {
                    return property;
                }
                std::cerr << "Initial value is of incompatible type" << std::endl;
            }
            // Any was not specified for property instance, create new one
            if (auto any = CreateAny(type)) {
                if (pi->SetAny(any)) {
                    return property;
                }
            }
        }
    }
    return {};
}

} // namespace strata

#ifndef EXT_META_OBJECT_H
#define EXT_META_OBJECT_H

#include <api/property.h>
#include <ext/metadata.h>
#include <ext/object.h>

/**
 * @brief CRTP base for LTK objects with static metadata.
 *
 * Extends Object with IMetaData support. Metadata is automatically collected
 * from all Interfaces that declare a `static constexpr metadata` member.
 * FinalClass may also define its own `metadata` member to override.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces the object implements.
 */
template<class FinalClass, class... Interfaces>
class MetaObject : public Object<FinalClass, IMetaData, Interfaces...>,
                   private MetaDataMixin<FinalClass>
{
public:
    /** @brief Collected metadata from all Interfaces. */
    static constexpr auto metadata = collected_metadata<Interfaces...>::value;

    MetaObject() = default;
    ~MetaObject() override = default;

    // IMetaData implementation
    array_view<MemberDesc> GetMembers() const override { return this->GetMembersImpl(); }
    IProperty::Ptr GetProperty(std::string_view name) const override { return this->GetPropertyImpl(name); }
    IEvent::Ptr GetEvent(std::string_view name) const override { return this->GetEventImpl(name); }
    IFunction::Ptr GetFunction(std::string_view name) const override { return this->GetFunctionImpl(name); }

public:
    static const IObjectFactory &GetFactory()
    {
        static Factory factory_;
        return factory_;
    }

private:
    class Factory : public ObjectFactory<FinalClass>
    {
        const ClassInfo &GetClassInfo() const override
        {
            static constexpr ClassInfo info{
                FinalClass::GetClassUid(),
                FinalClass::GetClassName(),
                {FinalClass::metadata.data(), FinalClass::metadata.size()}
            };
            return info;
        }
    };
};

#endif // EXT_META_OBJECT_H

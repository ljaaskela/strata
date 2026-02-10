#include "metadata_container.h"

namespace strata {

MetadataContainer::MetadataContainer(array_view<MemberDesc> members, const IStrata &instance)
    : members_(members), instance_(instance)
{}

array_view<MemberDesc> MetadataContainer::GetStaticMetadata() const
{
    return members_;
}

IProperty::Ptr MetadataContainer::GetProperty(std::string_view name) const
{
    for (auto& [n, p] : properties_) {
        if (n == name) return p;
    }
    for (auto& desc : members_) {
        if (desc.kind == MemberKind::Property && desc.name == name) {
            if (auto p = instance_.CreateProperty(desc.typeUid, {})) {
                properties_.push_back({name, p});
                return p;
            }
        }
    }
    return {};
}

IEvent::Ptr MetadataContainer::GetEvent(std::string_view name) const
{
    for (auto& [n, e] : events_) {
        if (n == name) return e;
    }
    for (auto& desc : members_) {
        if (desc.kind == MemberKind::Event && desc.name == name) {
            if (auto e = instance_.Create<IEvent>(ClassId::Event)) {
                events_.push_back({name, e});
                return e;
            }
        }
    }
    return {};
}

IFunction::Ptr MetadataContainer::GetFunction(std::string_view name) const
{
    for (auto& [n, f] : functions_) {
        if (n == name) return f;
    }
    for (auto& desc : members_) {
        if (desc.kind == MemberKind::Function && desc.name == name) {
            if (auto f = instance_.Create<IFunction>(ClassId::Function)) {
                functions_.push_back({name, f});
                return f;
            }
        }
    }
    return {};
}

} // namespace strata

#include "metadata_container.h"

namespace strata {

MetadataContainer::MetadataContainer(array_view<MemberDesc> members, const IStrata &instance)
    : members_(members), instance_(instance)
{}

array_view<MemberDesc> MetadataContainer::get_static_metadata() const
{
    return members_;
}

IProperty::Ptr MetadataContainer::get_property(std::string_view name) const
{
    for (auto& [n, p] : properties_) {
        if (n == name) return p;
    }
    for (auto& desc : members_) {
        if (desc.kind == MemberKind::Property && desc.name == name) {
            if (auto p = instance_.create_property(desc.typeUid, {})) {
                properties_.push_back({name, p});
                return p;
            }
        }
    }
    return {};
}

IEvent::Ptr MetadataContainer::get_event(std::string_view name) const
{
    for (auto& [n, e] : events_) {
        if (n == name) return e;
    }
    for (auto& desc : members_) {
        if (desc.kind == MemberKind::Event && desc.name == name) {
            if (auto e = instance_.create<IEvent>(ClassId::Event)) {
                events_.push_back({name, e});
                return e;
            }
        }
    }
    return {};
}

IFunction::Ptr MetadataContainer::get_function(std::string_view name) const
{
    for (auto& [n, f] : functions_) {
        if (n == name) return f;
    }
    for (auto& desc : members_) {
        if (desc.kind == MemberKind::Function && desc.name == name) {
            if (auto f = instance_.create<IFunction>(ClassId::Function)) {
                functions_.push_back({name, f});
                return f;
            }
        }
    }
    return {};
}

} // namespace strata

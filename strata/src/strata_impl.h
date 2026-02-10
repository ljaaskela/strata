#ifndef STRATA_IMPL_H
#define STRATA_IMPL_H

#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_strata.h>
#include <map>

namespace strata {

class StrataImpl final : public CoreObject<StrataImpl, IStrata>
{
public:
    StrataImpl();

    ReturnValue RegisterType(const IObjectFactory &factory) override;
    ReturnValue UnregisterType(const IObjectFactory &factory) override;
    IInterface::Ptr Create(Uid uid) const override;

    const ClassInfo* GetClassInfo(Uid classUid) const override;
    IAny::Ptr CreateAny(Uid type) const override;
    IProperty::Ptr CreateProperty(Uid type, const IAny::Ptr& value) const override;

private:
    std::map<Uid, const IObjectFactory *> types_;
};

} // namespace strata

#endif // STRATA_IMPL_H

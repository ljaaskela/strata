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

    ReturnValue register_type(const IObjectFactory &factory) override;
    ReturnValue unregister_type(const IObjectFactory &factory) override;
    IInterface::Ptr create(Uid uid) const override;

    const ClassInfo* get_class_info(Uid classUid) const override;
    IAny::Ptr create_any(Uid type) const override;
    IProperty::Ptr create_property(Uid type, const IAny::Ptr& value) const override;

private:
    std::map<Uid, const IObjectFactory *> types_;
};

} // namespace strata

#endif // STRATA_IMPL_H

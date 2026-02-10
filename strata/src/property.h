#ifndef PROPERTY_H
#define PROPERTY_H

#include "strata_impl.h"
#include <common.h>
#include <ext/event.h>
#include <ext/core_object.h>
#include <interface/intf_property.h>
#include <interface/types.h>

namespace strata {

class PropertyImpl final : public CoreObject<PropertyImpl, IProperty, IPropertyInternal>
{
public:
    PropertyImpl() = default;

protected: // IProperty
    ReturnValue set_value(const IAny &from) override;
    const IAny::ConstPtr get_value() const override;
    IEvent::Ptr on_changed() const override { return onChanged_; }

protected: // IPropertyInternal
    bool set_any(const IAny::Ptr &value) override;
    IAny::Ptr get_any() const override;

private:
    IAny::Ptr data_;
    LazyEvent onChanged_;
};

} // namespace strata

#endif // PROPERTY_H

#ifndef PROPERTY_H
#define PROPERTY_H

#include "metadata_item.h"

#include <velk/interface/intf_property.h>
#include <velk/interface/types.h>

namespace velk::impl {

/**
 * @brief Default IProperty/IPropertyInternal implementation.
 *
 * Stores a type-erased value via an IAny pointer in MemberSlot. Fires on_changed
 * when set_value/set_data modifies the value. Supports read-only mode via
 * ObjectFlags::ReadOnly. When the backing IAny implements IExternalAny,
 * automatically relays its on_data_changed event to the property's on_changed.
 */
class Property final : public MetadataItem<Property, IPropertyInternal>
{
public:
    VELK_CLASS_UID(ClassId::Property, "Property");

    Property() = default;
    ~Property() override;

protected: // IProperty
    ReturnValue set_value(const IAny& from, InvokeType type = Auto) override;
    const IAny::ConstPtr get_value() const override;
    IEvent::Ptr on_changed() const override
    {
        return is_standalone() ? IEvent::Ptr{}
                               : ::velk::get_property_event(get_owner(), get_storage_id(), Resolve::Create);
    }

protected: // IPropertyInternal
    bool set_any(const IAny::Ptr& value, IAny::Ptr* previous = nullptr) override;
    IAny::ConstPtr get_any() const override;
    ReturnValue set_data(const void* data, size_t size, Uid type, InvokeType invokeType = Auto) override;
    ReturnValue set_value_silent(const IAny& from) override;
    bool install_extension(const IAnyExtension::Ptr& extension) override;
    bool remove_extension(const IAnyExtension::Ptr& extension) override;

    void invoke_on_changed() const;
    IAny::Ptr& slot_any() const { return slot().data.property.data; }

private:
    bool external_{}; ///< True if data_ implements IExternalAny (on_data_changed fires on_changed
                      ///< automatically).
};

} // namespace velk::impl

#endif // PROPERTY_H

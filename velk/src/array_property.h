#ifndef ARRAY_PROPERTY_H
#define ARRAY_PROPERTY_H

#include "metadata_item.h"

#include <velk/interface/intf_any_extension.h>
#include <velk/interface/intf_array_any.h>
#include <velk/interface/intf_array_property.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/types.h>

namespace velk::impl {

/**
 * @brief Default IArrayProperty + IPropertyInternal implementation.
 *
 * Nearly identical to Property but also implements IArrayProperty by
 * delegating element-level operations to IArrayAny on its backing slot data.
 */
class ArrayProperty final : public MetadataItem<ArrayProperty, IPropertyInternal, IArrayProperty>
{
public:
    VELK_CLASS_UID(ClassId::ArrayProperty, "ArrayProperty");

    ArrayProperty() = default;
    ~ArrayProperty() override;

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

protected: // IArrayProperty
    size_t array_size() const override;
    ReturnValue get_at(size_t index, IAny& out) const override;
    ReturnValue set_at(size_t index, const IAny& value) override;
    ReturnValue push_back(const IAny& value) override;
    ReturnValue erase_at(size_t index) override;
    void clear_array() override;

    IAny::Ptr& slot_any() const { return slot().data.property.data; }

private:
    IArrayAny* get_array_any() const;
};

} // namespace velk::impl

#endif // ARRAY_PROPERTY_H

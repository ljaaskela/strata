#ifndef PROPERTY_H
#define PROPERTY_H

#include <velk/common.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_object_storage.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/types.h>

namespace velk::impl {

/**
 * @brief Default IProperty/IPropertyInternal implementation.
 *
 * Stores a type-erased value via an IAny pointer. Fires on_changed when
 * set_value/set_data modifies the value. Supports read-only mode via
 * ObjectFlags::ReadOnly. When the backing IAny implements IExternalAny,
 * automatically relays its on_data_changed event to the property's on_changed.
 */
class Property final : public ext::ObjectCore<Property, IPropertyInternal>
{
public:
    VELK_CLASS_UID(ClassId::Property, "Property");

    Property() = default;

public: // IStorageOwned
    IObjectStorage* get_owner() const override { return owner_; }
    size_t get_storage_id() const override { return storage_id_; }
    void set_owner(IInterface* storage, size_t id) override
    {
        owner_ = interface_cast<IObjectStorage>(storage);
        storage_id_ = id;
    }

protected: // IProperty
    ReturnValue set_value(const IAny& from, InvokeType type = Auto) override;
    const IAny::ConstPtr get_value() const override;
    IEvent::Ptr on_changed() const override
    {
        return ::velk::get_property_event(owner_, storage_id_, Resolve::Create);
    }

protected: // IPropertyInternal
    bool set_any(const IAny::Ptr& value, IAny::Ptr* previous = nullptr) override;
    IAny::ConstPtr get_any() const override;
    ReturnValue set_data(const void* data, size_t size, Uid type, InvokeType invokeType = Auto) override;
    ReturnValue set_value_silent(const IAny& from) override;
    bool install_extension(const IAnyExtension::Ptr& extension) override;
    bool remove_extension(const IAnyExtension::Ptr& extension) override;

    void invoke_on_changed() const;

private:
    IAny::Ptr data_;
    IObjectStorage* owner_{};
    size_t storage_id_{};
    bool external_{}; ///< True if data_ implements IExternalAny (on_data_changed fires on_changed
                      ///< automatically).
};

} // namespace velk::impl

#endif // PROPERTY_H

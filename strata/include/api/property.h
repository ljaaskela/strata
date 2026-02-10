#ifndef API_PROPERTY_H
#define API_PROPERTY_H

#include <api/any.h>
#include <api/function.h>
#include <common.h>
#include <interface/intf_property.h>
#include <interface/intf_strata.h>
#include <interface/types.h>
#include <iostream>

namespace strata {

/** @brief Convenience wrapper around an IProperty::Ptr with event subscription helpers. */
class Property
{
public:
    operator const IProperty::Ptr()
    {
        return prop_;
    }
    operator const IProperty::ConstPtr() const
    {
        return prop_;
    }
    /** @brief Returns true if the underlying IProperty is valid. */
    operator bool() const
    {
        return prop_.operator bool();
    }
    /** @brief Returns the underlying IProperty pointer. */
    const IProperty::Ptr get_property_interface()
    {
        return prop_;
    }
    /** @copydoc get_property_interface() */
    const IProperty::ConstPtr get_property_interface() const
    {
        return prop_;
    }
    /** @brief Subscribes @p fn to be called when the property value changes. */
    void add_on_changed(const Function &fn)
    {
        if (prop_) {
            prop_->on_changed()->add_handler(fn);
        }
    }
    /** @brief Unsubscribes @p fn from property change notifications. */
    void remove_on_changed(const Function &fn)
    {
        if (prop_) {
            prop_->on_changed()->remove_handler(fn);
        }
    }

    /** @brief Returns the UID of the value type stored by this property. */
    virtual Uid get_type_uid() const = 0;

protected:
    void create()
    {
        prop_ = instance().create_property(get_type_uid(), {});
        if (prop_) {
            internal_ = prop_->get_interface<IPropertyInternal>();
        }
    }

    Property() = default;
    explicit Property(IProperty::Ptr existing) : prop_(std::move(existing))
    {
        if (prop_) {
            internal_ = prop_->get_interface<IPropertyInternal>();
        }
    }
    IProperty::Ptr prop_;
    IPropertyInternal *internal_{};
};

/**
 * @brief Typed property wrapper with get_value/set_value accessors for type T.
 * @tparam T The value type stored by the property.
 */
template<class T>
class PropertyT final : public Property
{
public:
    static constexpr Uid TYPE_UID = type_uid<T>();
    /** @brief Default-constructs a property of type T via Strata. */
    PropertyT()
    {
        create();
    }
    /** @brief Constructs a property of type T and sets its initial value. */
    PropertyT(const T& value)
    {
        create();
        set_value(value);
    }
    /** @brief Wraps an existing IProperty pointer. */
    explicit PropertyT(IProperty::Ptr existing) : Property(std::move(existing)) {}
    Uid get_type_uid() const override
    {
        return TYPE_UID;
    }
    /** @brief Returns the current value of the property. */
    T get_value() const {
        if (internal_) {
            // Typed accessor reference to property's any
            return AnyT<T>(internal_->get_any()).get_value();
        }
        return {};
    }
    /** @brief Sets the property to @p value. */
    void set_value(const T& value) {
        if (internal_) {
            // This is a bit suboptimal as we create a new any object to wrap the value
            auto v = AnyT<T>(value);
            prop_->set_value(v);
        }
    }
};

} // namespace strata

#endif // PROPERTY_H

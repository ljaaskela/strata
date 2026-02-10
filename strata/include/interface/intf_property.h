#ifndef INTF_PROPERTY_H
#define INTF_PROPERTY_H

#include <interface/intf_any.h>
#include <interface/intf_event.h>

namespace strata {

/** @brief Interface for a type-erased property with change notification. */
class IProperty : public Interface<IProperty>
{
public:
    /**
     * @brief Sets the property to a given value
     * @param from The value to set from.
     * @return ReturnValue::SUCCESS if value changed
     *         ReturnValue::NOTHING_TO_DO if the same value was set
     *         ReturnValue::FAIL otherwise
     */
    virtual ReturnValue set_value(const IAny &from) = 0;
    /**
     * @brief Returns the property's current value.
     */
    virtual const IAny::ConstPtr get_value() const = 0;
    /**
     * @brief Invoked when value of the property changes as a response to
     *        set_value being called.
     */
    virtual IEvent::Ptr on_changed() const = 0;
};

/** @brief Internal interface for initializing a property's backing IAny storage. */
class IPropertyInternal : public Interface<IPropertyInternal>
{
public:
    /**
     * @brief Sets the internal any object for a property instance.
     * @param any The any object to set.
     * @note  This function can be called only once to initialize the property.
     *        Internal any object cannot be changed after initialization.
     */
    virtual bool set_any(const IAny::Ptr &any) = 0;
    /**
     * @brief Returns the internal any object.
     * @note  Any changes through the direct any accessor will not lead to
     *        IProperty::on_changed being invoked.
     */
    virtual IAny::Ptr get_any() const = 0;
};

} // namespace strata

#endif

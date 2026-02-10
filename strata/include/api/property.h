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
    const IProperty::Ptr GetPropertyInterface()
    {
        return prop_;
    }
    /** @copydoc GetPropertyInterface() */
    const IProperty::ConstPtr GetPropertyInterface() const
    {
        return prop_;
    }
    /** @brief Subscribes @p fn to be called when the property value changes. */
    void AddOnChanged(const Function &fn)
    {
        if (prop_) {
            prop_->OnChanged()->AddHandler(fn);
        }
    }
    /** @brief Unsubscribes @p fn from property change notifications. */
    void RemoveOnChanged(const Function &fn)
    {
        if (prop_) {
            prop_->OnChanged()->RemoveHandler(fn);
        }
    }

    /** @brief Returns the UID of the value type stored by this property. */
    virtual Uid GetTypeUid() const = 0;

protected:
    void Create()
    {
        prop_ = Strata().CreateProperty(GetTypeUid(), {});
        if (prop_) {
            internal_ = prop_->GetInterface<IPropertyInternal>();
        }
    }

    Property() = default;
    explicit Property(IProperty::Ptr existing) : prop_(std::move(existing))
    {
        if (prop_) {
            internal_ = prop_->GetInterface<IPropertyInternal>();
        }
    }
    IProperty::Ptr prop_;
    IPropertyInternal *internal_{};
};

/**
 * @brief Typed property wrapper with Get/Set accessors for type T.
 * @tparam T The value type stored by the property.
 */
template<class T>
class PropertyT final : public Property
{
public:
    static constexpr Uid TYPE_UID = TypeUid<T>();
    /** @brief Default-constructs a property of type T via Strata. */
    PropertyT()
    {
        Create();
    }
    /** @brief Constructs a property of type T and sets its initial value. */
    PropertyT(const T& value)
    {
        Create();
        Set(value);
    }
    /** @brief Wraps an existing IProperty pointer. */
    explicit PropertyT(IProperty::Ptr existing) : Property(std::move(existing)) {}
    Uid GetTypeUid() const override
    {
        return TYPE_UID;
    }
    /** @brief Returns the current value of the property. */
    T Get() const {
        if (internal_) {
            // Typed accessor reference to property's any
            return AnyT<T>(internal_->GetAny()).Get();
        }
        return {};
    }
    /** @brief Sets the property to @p value. */
    void Set(const T& value) {
        if (internal_) {
            // This is a bit suboptimal as we create a new any object to wrap the value
            auto v = AnyT<T>(value);
            prop_->SetValue(v);
        }
    }
};

} // namespace strata

#endif // PROPERTY_H

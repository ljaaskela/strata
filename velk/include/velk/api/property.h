#ifndef VELK_API_PROPERTY_H
#define VELK_API_PROPERTY_H

#include <velk/api/any.h>
#include <velk/api/object_ref.h>
#include <velk/api/variant.h>
#include <velk/api/velk.h>
#include <velk/common.h>
#include <velk/interface/intf_array_property.h>
#include <velk/interface/intf_function.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_velk.h>
#include <velk/interface/types.h>
#include <velk/vector.h>

namespace velk {

namespace detail {

/** @brief Non-template storage for ConstProperty<T>. Compiled once regardless of T instantiations. */
class PropertyStorage
{
protected:
    PropertyStorage() = delete;
    explicit PropertyStorage(IProperty::Ptr existing) : prop_(std::move(existing)) {}

public:
    /** @brief Returns wrapped IProperty interface. */
    operator const IProperty::Ptr() { return prop_; }

    /** @brief Returns wrapped IProperty interface. */
    operator const IProperty::ConstPtr() const { return prop_; }

    /** @brief Returns true if the underlying IProperty is valid. */
    operator bool() const { return prop_.operator bool(); }

    /** @brief Subscribes @p fn to be called when the property value changes. */
    void add_on_changed(const IFunction::ConstPtr& fn)
    {
        if (prop_) {
            prop_->on_changed()->add_handler(fn);
        }
    }

    /** @brief Unsubscribes @p fn from property change notifications. */
    void remove_on_changed(const IFunction::ConstPtr& fn)
    {
        if (prop_) {
            prop_->on_changed()->remove_handler(fn);
        }
    }

protected:
    IPropertyInternal* get_internal() const { return interface_cast<IPropertyInternal>(prop_); }
    IProperty::Ptr prop_;
};

} // namespace detail

/**
 * @brief Read-only typed property wrapper with get_value accessor for type T.
 *
 * Returned by RPROP accessors. Provides read and observe operations but no set_value.
 * @tparam T The value type stored by the property.
 */
template <class T>
class ConstProperty : public detail::PropertyStorage
{
public:
    using Type = std::remove_const_t<T>;
    static constexpr Uid TYPE_UID = type_uid<Type>();

    /** @brief Wraps an existing IProperty pointer. */
    explicit ConstProperty(IProperty::Ptr existing) : PropertyStorage(std::move(existing)) {}

    /** @brief Returns the current value of the property. */
    T get_value() const
    {
        Type value{};
        if (auto internal = get_internal()) {
            if (auto any = internal->get_any()) {
                any->get_data(&value, sizeof(Type), TYPE_UID);
            }
        }
        return value;
    }
};

/**
 * @brief Typed property wrapper with get_value/set_value accessors for type T.
 *
 * Returned by PROP accessors. Inherits read/observe from ConstProperty<T> and adds set_value.
 * @tparam T The value type stored by the property.
 */
template <class T>
class Property : public ConstProperty<T>
{
    using Base = ConstProperty<T>;
    using Type = typename Base::Type;

public:
    /** @brief Wraps an existing IProperty pointer. */
    explicit Property(IProperty::Ptr existing) : Base(std::move(existing)) {}

    /** @brief Sets the property to @p value. */
    ReturnValue set_value(const Type& value, InvokeType type = Auto)
    {
        if (auto internal = this->get_internal()) {
            return internal->set_data(&value, sizeof(Type), Base::TYPE_UID, type);
        }
        return ReturnValue::Fail;
    }
};

/**
 * @brief Read-only typed array property wrapper with element access.
 *
 * Returned by RARR accessors. Provides read-only element access without copying the full vector.
 * @tparam T The element type.
 */
template <class T>
class ConstArrayProperty : public detail::PropertyStorage
{
public:
    using Type = std::remove_const_t<T>;

    /** @brief Wraps an existing IProperty pointer (must implement IArrayProperty). */
    explicit ConstArrayProperty(IProperty::Ptr existing) : PropertyStorage(std::move(existing)) {}

    /** @brief Returns the number of elements. */
    size_t size() const { return arr().size(); }

    /** @brief Returns true if the array is empty. */
    bool empty() const { return arr().empty(); }

    /** @brief Returns the element at @p index. Returns default-constructed T on failure. */
    T at(size_t index) const { return arr().at(index); }

    /** @brief Returns a full copy of the vector. */
    vector<Type> get_value() const { return arr().get_value(); }

protected:
    ArrayAny<const Type> arr() const { return ArrayAny<const Type>(prop_ ? prop_->get_value() : nullptr); }
};

/**
 * @brief Typed array property wrapper with element-level mutation.
 *
 * Returned by ARR accessors. Inherits read from ConstArrayProperty<T> and adds mutation.
 * @tparam T The element type.
 */
template <class T>
class ArrayProperty : public ConstArrayProperty<T>
{
    using Base = ConstArrayProperty<T>;
    using Type = typename Base::Type;

public:
    /** @brief Wraps an existing IProperty pointer (must implement IArrayProperty). */
    explicit ArrayProperty(IProperty::Ptr existing) : Base(std::move(existing)) {}

    /** @brief Sets the element at @p index to @p value. */
    ReturnValue set_at(size_t index, const Type& value)
    {
        if (auto* ap = get_array_prop()) {
            ext::AnyValue<Type> av;
            av.set_value(value);
            return ap->set_at(index, av);
        }
        return ReturnValue::Fail;
    }

    /** @brief Appends @p value to the end of the array. */
    ReturnValue push_back(const Type& value)
    {
        if (auto* ap = get_array_prop()) {
            ext::AnyValue<Type> av;
            av.set_value(value);
            return ap->push_back(av);
        }
        return ReturnValue::Fail;
    }

    /** @brief Erases the element at @p index. */
    ReturnValue erase_at(size_t index)
    {
        if (auto* ap = get_array_prop()) {
            return ap->erase_at(index);
        }
        return ReturnValue::Fail;
    }

    /** @brief Removes all elements. */
    void clear()
    {
        if (auto* ap = get_array_prop()) {
            ap->clear_array();
        }
    }

    /** @brief Sets the whole vector value. */
    ReturnValue set_value(const vector<Type>& value, InvokeType type = Auto)
    {
        if (this->prop_) {
            Any<vector<Type>> av(value);
            return this->prop_->set_value(av, type);
        }
        return ReturnValue::Fail;
    }

private:
    IArrayProperty* get_array_prop() const { return interface_cast<IArrayProperty>(this->prop_); }
};

/**
 * @brief Read-only variant property wrapper. Returns the backing IAny directly.
 *
 * Returned by RPROP accessors when the type is Variant.
 */
template <>
class ConstProperty<Variant> : public detail::PropertyStorage
{
public:
    explicit ConstProperty(IProperty::Ptr existing) : PropertyStorage(std::move(existing)) {}

    /** @brief Returns the backing IAny (the VariantAny itself). */
    IAny::ConstPtr get_value() const
    {
        if (auto internal = get_internal()) {
            return internal->get_any();
        }
        return {};
    }
};

/**
 * @brief Read-write variant property wrapper. Accepts any IAny value.
 *
 * Returned by PROP accessors when the type is Variant.
 */
template <>
class Property<Variant> : public ConstProperty<Variant>
{
public:
    explicit Property(IProperty::Ptr existing) : ConstProperty<Variant>(std::move(existing)) {}

    /** @brief Sets the property from a raw IAny reference. */
    ReturnValue set_value(const IAny& value, InvokeType type = Auto)
    {
        return prop_ ? prop_->set_value(value, type) : ReturnValue::Fail;
    }

    /** @brief Sets the property from an IAny shared pointer. */
    ReturnValue set_value(const IAny::ConstPtr& value, InvokeType type = Auto)
    {
        return (prop_ && value) ? prop_->set_value(*value, type) : ReturnValue::Fail;
    }
};

/**
 * @brief Read-only object ref property wrapper. Returns the backing IAny directly.
 *
 * Returned by RPROP accessors when the type is ObjectRef.
 */
template <>
class ConstProperty<ObjectRef> : public detail::PropertyStorage
{
public:
    explicit ConstProperty(IProperty::Ptr existing) : PropertyStorage(std::move(existing)) {}

    /** @brief Returns the backing IAny (the ObjectRefImpl itself). */
    IAny::ConstPtr get_value() const
    {
        if (auto internal = get_internal()) {
            return internal->get_any();
        }
        return {};
    }
};

/**
 * @brief Read-write object ref property wrapper. Accepts any IAny value.
 *
 * Returned by PROP accessors when the type is ObjectRef.
 */
template <>
class Property<ObjectRef> : public ConstProperty<ObjectRef>
{
public:
    explicit Property(IProperty::Ptr existing) : ConstProperty<ObjectRef>(std::move(existing)) {}

    /** @brief Sets the property from a raw IAny reference. */
    ReturnValue set_value(const IAny& value, InvokeType type = Auto)
    {
        return prop_ ? prop_->set_value(value, type) : ReturnValue::Fail;
    }

    /** @brief Sets the property from an IAny shared pointer. */
    ReturnValue set_value(const IAny::ConstPtr& value, InvokeType type = Auto)
    {
        return (prop_ && value) ? prop_->set_value(*value, type) : ReturnValue::Fail;
    }
};

/**
 * @brief A helper template for creating a new read-only poperty instance
 */
template <class T, class = std::enable_if_t<std::is_const_v<T>>>
ConstProperty<std::remove_const_t<T>> create_property(const T& value = {})
{
    using Type = std::remove_const_t<T>;
    Any<Type> v(value);
    return ConstProperty<Type>(instance().create_property<Type>(v.clone(), ObjectFlags::ReadOnly));
}

/**
 * @brief A helper template for creating a new poperty instance
 */
template <class T, class = std::enable_if_t<!std::is_const_v<T>>>
Property<T> create_property(const T& value = {})
{
    Any<T> v(value);
    return Property<T>(instance().create_property<T>(v.clone()));
}

} // namespace velk

#endif // VELK_API_PROPERTY_H

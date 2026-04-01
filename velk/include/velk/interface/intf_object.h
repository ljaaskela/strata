#ifndef VELK_INTF_OBJECT_H
#define VELK_INTF_OBJECT_H

#include <velk/common.h>
#include <velk/interface/intf_interface.h>
#include <velk/interface/types.h>
#include <velk/string.h>

namespace velk {

/** @brief Base interface for all Velk objects. */
class IObject : public Interface<IObject>
{
public:
    /** @brief Returns the class UID of this object.
     *  @note Never changes.
     */
    virtual Uid get_class_uid() const = 0;

    /** @brief Returns the name of the class.
     *  @note Never changes. */
    virtual string_view get_class_name() const = 0;

    /** @brief Returns the instance name of this object, or empty if unnamed.
     *  @note Can change between calls. */
    virtual string get_name() const = 0;

    /** @brief Returns a shared_ptr to this object, or empty if not available. */
    virtual Ptr get_self() const = 0;

    /** @brief Returns the object's flags (bitwise combination of ObjectFlags). */
    virtual uint32_t get_object_flags() const = 0;

    /**
     * @brief Returns a shared_ptr to this object, cast to interface T.
     * @tparam T The target interface type.
     */
    template <class T>
    typename T::Ptr get_self() const
    {
        return interface_pointer_cast<T>(get_self());
    }
};

/**
 * @brief Returns a shared_ptr to the object, optionally cast to interface T.
 * @tparam T The target interface type (defaults to IObject).
 * @param object The object to retrieve the self pointer from.
 */
template <class T = IObject, class U>
typename T::Ptr get_self(U* object)
{
    auto* obj = interface_cast<IObject>(object);
    return obj ? interface_pointer_cast<T>(obj->get_self()) : typename T::Ptr{};
}

/** @brief Converts any interface shared_ptr to IObject::Ptr. */
template <class T>
IObject::Ptr as_object(const shared_ptr<T>& ptr)
{
    return interface_pointer_cast<IObject>(ptr);
}

/** @brief Returns the name of an object. */
template <class T>
::velk::string get_name(const shared_ptr<T>& ptr)
{
    auto* obj = interface_cast<IObject>(ptr);
    return obj ? obj->get_name() : ::velk::string{};
}

} // namespace velk

#endif // VELK_INTF_OBJECT_H

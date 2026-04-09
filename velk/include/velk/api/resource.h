#ifndef VELK_API_RESOURCE_H
#define VELK_API_RESOURCE_H

#include <velk/api/object.h>
#include <velk/interface/resource/intf_resource.h>

namespace velk {

/**
 * @brief Convenience wrapper around IResource.
 *
 * Provides null-safe accessors for the resource URI, existence, size,
 * and persistence. Subclassed by typed resource wrappers (Image,
 * Environment, Font, etc.).
 *
 * @code
 *   auto res = store.get_resource("image:app://logo.png");
 *   Resource r(interface_pointer_cast<IObject>(res));
 *   if (r.exists()) {
 *       auto uri = r.get_uri();
 *   }
 * @endcode
 */
class Resource : public Object
{
public:
    /** @brief Default-constructed Resource wraps no object. */
    Resource() = default;

    /** @brief Wraps an existing IObject pointer, rejected if it does not implement IResource. */
    explicit Resource(IObject::Ptr obj) : Object(check_object<IResource>(obj)) {}

    /** @brief Wraps an existing IResource pointer. */
    explicit Resource(IResource::Ptr r) : Object(as_object(r)) {}

    /** @brief Implicit conversion to IResource::Ptr. */
    operator IResource::Ptr() const { return as_ptr<IResource>(); }

    /** @brief Returns the full URI of the resource. */
    string_view get_uri() const
    {
        return with<IResource>([](auto& r) { return r.uri(); });
    }

    /** @brief Returns true if the resource exists (was loaded successfully). */
    bool exists() const
    {
        return with<IResource>([](auto& r) { return r.exists(); });
    }

    /** @brief Returns the size of the resource in bytes, or -1 on failure. */
    int64_t get_size() const
    {
        return with_or<IResource>([](auto& r) { return r.size(); }, int64_t{-1});
    }

    /** @brief Returns whether this resource is pinned in the cache. */
    bool is_persistent() const
    {
        return with<IResource>([](auto& r) { return r.is_persistent(); });
    }

    /** @brief Sets the persistence flag. */
    void set_persistent(bool value)
    {
        with<IResource>([value](auto& r) { r.set_persistent(value); });
    }
};

} // namespace velk

#endif // VELK_API_RESOURCE_H

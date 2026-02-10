#ifndef INTF_INTERFACE_H
#define INTF_INTERFACE_H

#include <common.h>

namespace strata {

/** @brief Mixin (inherit to add behavior) that deletes copy constructor and copy assignment operator. */
struct NoCopy
{
    NoCopy() = default;
    ~NoCopy() = default;
    NoCopy(const NoCopy &) = delete;
    NoCopy &operator=(const NoCopy &) = delete;
};

/** @brief Mixin (inherit to add behavior) that deletes move constructor and move assignment operator. */
struct NoMove
{
    NoMove() = default;
    ~NoMove() = default;
    NoMove(NoMove &&) = delete;
    NoMove &operator=(NoMove &&) = delete;
};

/** @brief Mixin (inherit to add behavior) that prevents both copying and moving. */
struct NoCopyMove : public NoCopy, public NoMove
{
    NoCopyMove() = default;
    ~NoCopyMove() = default;
};

/** @brief Static descriptor for an interface type, providing its UID and name. */
struct InterfaceInfo {
    Uid uid;
    std::string_view name;
};

/**
 * @brief Root interface for all Strata interfaces.
 *
 * Provides UID-based interface querying and manual reference counting.
 * All concrete interfaces derive from this through the Interface<T> CRTP template.
 */
class IInterface : NoCopyMove
{
public:
    /** @brief UID for the root interface (default-constructed, matches nothing specific). */
    static constexpr Uid UID{};
    /** @brief Shared pointer to a mutable IInterface. */
    using Ptr = std::shared_ptr<IInterface>;
    /** @brief Shared pointer to a const IInterface. */
    using ConstPtr = std::shared_ptr<const IInterface>;
    /** @brief Weak pointer to a mutable IInterface. */
    using WeakPtr = std::weak_ptr<IInterface>;
    /** @brief Weak pointer to a const IInterface. */
    using ConstWeakPtrr = std::weak_ptr<const IInterface>;

public:
    /** @brief Returns a pointer to the requested interface, or nullptr if not supported. */
    virtual IInterface* get_interface(Uid uid) = 0;
    /** @copydoc get_interface(Uid) */
    virtual const IInterface *get_interface(Uid uid) const = 0;

    /**
     * @brief Type-safe interface query.
     * @tparam T The interface type to query for. Must have a static UID member.
     */
    template<class T>
    T *get_interface() noexcept
    {
        return static_cast<T*>(get_interface(T::UID));
    }
    /** @copydoc get_interface() */
    template<class T>
    const T *get_interface() const noexcept
    {
        return static_cast<const T *>(get_interface(T::UID));
    }

    /** @brief Increments the reference count. */
    virtual void ref() = 0;
    /** @brief Decrements the reference count. May delete the object when it reaches zero. */
    virtual void unref() = 0;

protected:
    IInterface() = default;
    virtual ~IInterface() = default;
};

#include <interface/refcnt_ptr.h>

/**
 * @brief CRTP base template for defining concrete Strata interfaces.
 *
 * Provides automatic UID generation from the type name, and standard smart pointer
 * type aliases (Ptr, ConstPtr, WeakPtr, ConstWeakPtr, RefPtr).
 *
 * @tparam T The derived interface type (CRTP parameter).
 */
template<typename T>
class Interface : public IInterface
{
public:
    /** @brief Compile-time unique identifier for this interface type. */
    static constexpr Uid UID = type_uid<T>();
    /** @brief Static descriptor containing the UID and human-readable name. */
    static constexpr InterfaceInfo INFO { type_uid<T>(), get_name<T>() };
    /** @brief Shared pointer to a mutable T. */
    using Ptr = std::shared_ptr<T>;
    /** @brief Shared pointer to a const T. */
    using ConstPtr = std::shared_ptr<const T>;
    /** @brief Weak pointer to a mutable T. */
    using WeakPtr = std::weak_ptr<T>;
    /** @brief Weak pointer to a const T. */
    using ConstWeakPtr = std::weak_ptr<const T>;
    /** @brief Intrusive reference-counting pointer to T. */
    using RefPtr = refcnt_ptr<T>;
    using IInterface::get_interface;

protected:
    Interface() = default;
    ~Interface() override = default;
};

} // namespace strata

#endif // INTF_INTERFACE_H

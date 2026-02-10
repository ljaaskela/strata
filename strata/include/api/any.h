#ifndef API_ANY_H
#define API_ANY_H

#include <api/strata.h>
#include <cassert>
#include <common.h>
#include <interface/intf_any.h>
#include <interface/intf_strata.h>

namespace strata {

/** @brief Read-only wrapper around an IAny pointer with reference-counted ownership. */
class ConstAny
{
public:
    /** @brief Implicit conversion to a const IAny pointer. */
    operator const IAny *() const noexcept { return any_.get(); }
    /** @brief Returns the underlying const IAny pointer. */
    const IAny *GetAnyInterface() const noexcept { return any_.get(); }
    /** @brief Returns true if the wrapper holds a valid IAny. */
    operator bool() const noexcept { return any_.operator bool(); }

protected:
    ConstAny() = default;
    virtual ~ConstAny() = default;

    void SetAny(const IAny &any, const Uid &req) noexcept
    {
        if (IsCompatible(any, req)) {
            SetAnyDirect(any);
        }
    }
    void SetAny(const IAny::ConstPtr &any, const Uid &req) noexcept
    {
        if (IsCompatible(any, req)) {
            SetAnyDirect(any);
        }
    }
    void SetAnyDirect(const IAny &any) noexcept
    {
        any_ = refcnt_ptr<IAny>(const_cast<IAny *>(&any));
    }
    void SetAnyDirect(const IAny::ConstPtr &any) noexcept { SetAnyDirect(*(any.get())); }
    refcnt_ptr<IAny> any_;
};

/** @brief Read-write wrapper around an IAny pointer. */
class Any : public ConstAny
{
public:
    /** @brief Implicit conversion to a mutable IAny pointer. */
    operator IAny *() { return any_.get(); }
    /** @brief Copies the value from @p other into the managed IAny. */
    bool CopyFrom(const IAny &other) { return any_ && any_->CopyFrom(other); }
    /** @brief Returns the underlying mutable IAny pointer. */
    IAny *GetAnyInterface() { return any_.get(); }

protected:
    Any() = default;
};

/**
 * @brief Typed wrapper for IAny that provides Get/Set accessors for type T.
 *
 * Can be constructed from an existing IAny or will create a new one from Strata.
 *
 * @tparam T The value type. Use const T for read-only access.
 */
template<class T>
class AnyT final : public Any
{
    static constexpr bool IsReadWrite = !std::is_const_v<T>;
    static constexpr auto TYPE_SIZE = sizeof(T);

public:
    static constexpr Uid TYPE_UID = TypeUid<std::remove_const_t<T>>();

    /** @brief Wraps an existing mutable IAny pointer (read-write only). */
    template<class Flag = std::enable_if_t<IsReadWrite>>
    constexpr AnyT(const IAny::Ptr &any) noexcept
    {
        SetAny(any, TYPE_UID);
    }
    /** @brief Wraps an existing const IAny pointer. */
    constexpr AnyT(const IAny::ConstPtr &any) noexcept { SetAny(any, TYPE_UID); }
    /** @brief Wraps a const IAny reference. */
    constexpr AnyT(const IAny &any) noexcept { SetAny(any, TYPE_UID); }
    /** @brief Move-constructs from an IAny rvalue. */
    constexpr AnyT(IAny &&any) noexcept
    {
        if (IsCompatible(any, TYPE_UID)) {
            any_ = std::move(any);
        }
    }
    /** @brief Default-constructs an IAny of type T via Strata. */
    AnyT() noexcept { Create(); }
    /** @brief Constructs an IAny of type T and initializes it with @p value. */
    AnyT(const T &value) noexcept
    {
        if (!IsCompatible(any_, TYPE_UID)) {
            auto any = Strata().CreateAny(TYPE_UID);
            any_.reset(any.get());
        }
        any_->SetData(&value, TYPE_SIZE, TYPE_UID);
    }
    operator const IAny &() const noexcept { return *(any_.get()); }

    constexpr Uid GetTypeUid() const noexcept { return TYPE_UID; }
    /** @brief Returns a copy of the stored value. */
    T Get() const noexcept
    {
        std::remove_const_t<T> value{};
        if (any_) {
            any_->GetData(&value, sizeof(T), TYPE_UID);
        }
        return value;
    }
    /** @brief Overwrites the stored value with @p value. */
    void Set(const T &value) noexcept
    {
        if (any_) {
            any_->SetData(&value, sizeof(T), TYPE_UID);
        }
    }

    /** @brief Creates a read-write typed view over an existing IAny pointer. */
    static AnyT<T> Ref(const IAny::Ptr &ref) { return AnyT<T>(ref); }
    /** @brief Creates a read-only typed view over an existing const IAny pointer. */
    static const AnyT<const T> ConstRef(const IAny::ConstPtr &ref) { return AnyT<const T>(ref); }

protected:
    void Create() { SetAnyDirect(Strata().CreateAny(TYPE_UID)); }
};

} // namespace strata

#endif // ANY_H

#ifndef VELK_API_OBJECT_H
#define VELK_API_OBJECT_H

#include <velk/api/attachment.h>
#include <velk/api/state.h>
#include <velk/interface/intf_object_storage.h>
#include <velk/vector.h>

namespace velk {

/**
 * @brief Convenience wrapper around IObject::Ptr providing unified access
 * to the full object surface: metadata, state, attachments, and container.
 *
 * All methods are null-safe: calling them on a default-constructed (empty)
 * Object returns safe defaults (nullptr, 0, empty views, etc.).
 */
class Object
{
public:
    Object() = default;
    explicit Object(IObject::Ptr obj) : obj_(std::move(obj)) {}

    /** @brief Returns true if the underlying IObject is valid. */
    operator bool() const { return obj_.operator bool(); }

    /** @brief Returns the class UID of the object, or zero if invalid. */
    Uid class_uid() const { return obj_ ? obj_->get_class_uid() : Uid{}; }

    /** @brief Returns the class name of the object, or empty if invalid. */
    string_view class_name() const { return obj_ ? obj_->get_class_name() : string_view{}; }

    /** @brief Returns the object's flags, or 0 if invalid. */
    uint32_t flags() const { return obj_ ? obj_->get_object_flags() : 0; }

    /** @brief Casts the object to interface T (raw pointer). */
    template <class T>
    T* as() const
    {
        return interface_cast<T>(obj_);
    }

    /** @brief Casts the object to interface T (shared pointer). */
    template <class T>
    typename T::Ptr as_ptr() const
    {
        return obj_ ? interface_pointer_cast<T>(obj_) : typename T::Ptr{};
    }

    /** @brief Returns a property by name, optionally creating it on first access. */
    IProperty::Ptr get_property(string_view name, Resolve mode = Resolve::Create) const
    {
        auto* meta = as<IMetadata>();
        return meta ? meta->get_property(name, mode) : IProperty::Ptr{};
    }

    /** @brief Returns an event by name, optionally creating it on first access. */
    IEvent::Ptr get_event(string_view name, Resolve mode = Resolve::Create) const
    {
        auto* meta = as<IMetadata>();
        return meta ? meta->get_event(name, mode) : IEvent::Ptr{};
    }

    /** @brief Returns a function by name, optionally creating it on first access. */
    IFunction::Ptr get_function(string_view name, Resolve mode = Resolve::Create) const
    {
        auto* meta = as<IMetadata>();
        return meta ? meta->get_function(name, mode) : IFunction::Ptr{};
    }

    /** @brief Returns the static metadata descriptors for this object's class. */
    array_view<MemberDesc> static_metadata() const
    {
        auto* meta = as<IMetadata>();
        return meta ? meta->get_static_metadata() : array_view<MemberDesc>{};
    }

    /** @brief Read-only access to T::State. */
    template <class T>
    detail::StateReader<T> read_state() const
    {
        return obj_ ? ::velk::read_state<T>(obj_.get()) : detail::StateReader<T>();
    }

    /** @brief Write access to T::State (fires on_changed on destruction). */
    template <class T>
    detail::StateWriter<T> write_state()
    {
        return obj_ ? ::velk::write_state<T>(obj_.get()) : detail::StateWriter<T>();
    }

    /** @brief Writes to T::State via a callback, with optional deferral. */
    template <class T, class Fn>
    void write_state(Fn&& fn, InvokeType type = Auto)
    {
        if (obj_) {
            ::velk::write_state<T>(obj_.get(), std::forward<Fn>(fn), type);
        }
    }

    /** @brief Invokes a function by name with no arguments. */
    IAny::Ptr invoke_function(string_view name) const
    {
        return obj_ ? ::velk::invoke_function(obj_.get(), name) : nullptr;
    }

    /** @brief Invokes a function by name with multiple value arguments (auto-wrapped in Any). */
    template <class... Args, detail::require_value_args<Args...> = 0>
    IAny::Ptr invoke_function(string_view name, const Args&... args) const
    {
        return obj_ ? ::velk::invoke_function(obj_.get(), name, args...) : nullptr;
    }

    /** @brief Invokes an event by name with no arguments. */
    ReturnValue invoke_event(string_view name) const
    {
        return obj_ ? ::velk::invoke_event(obj_.get(), name) : ReturnValue::InvalidArgument;
    }

    /** @brief Adds an attachment to this object's storage. */
    ReturnValue add_attachment(const IInterface::Ptr& attachment)
    {
        return with_or<IObjectStorage>([&](auto& s) { return s.add_attachment(attachment); },
                                       ReturnValue::InvalidArgument);
    }

    /** @brief Removes an attachment by pointer identity. */
    ReturnValue remove_attachment(const IInterface::Ptr& attachment)
    {
        return with_or<IObjectStorage>([&](auto& s) { return s.remove_attachment(attachment); },
                                       ReturnValue::InvalidArgument);
    }

    /** @brief Returns the number of attachments, or 0 if invalid. */
    size_t attachment_count() const
    {
        return with<IObjectStorage>([](auto& s) { return s.attachment_count(); });
    }

    /** @brief Finds the first attachment implementing interface T. */
    template <class T>
    auto find_attachment() const
    {
        return with<IObjectStorage>([](auto& s) { return s.template find_attachment<T>(); });
    }
    /** @brief Finds the first attachment implementing interface T. */
    template <class T>
    vector<typename T::Ptr> find_attachments() const
    {
        auto att = with<IObjectStorage>([](auto& s) {
            AttachmentQuery q;
            q.interfaceUid = T::UID;
            return s.find_attachments(q);
        });
        vector<typename T::Ptr> res;
        for (auto&& a : att) {
            if (auto p = interface_pointer_cast<T>(a)) {
                res.emplace_back(std::move(p));
            }
        }
        return res;
    }

    /** @brief Finds or creates an attachment of type T with the given class UID. */
    template <class T>
    typename T::Ptr find_or_create_attachment(Uid classUid)
    {
        return with<IObjectStorage>([&](auto& s) { return s.template find_attachment<T>(classUid); });
    }

    /** @brief Returns the underlying IObject shared pointer. */
    IObject::Ptr get() const { return obj_; }

    /** @brief Raw access to the underlying IObject. */
    IObject* operator->() const { return obj_.get(); }

protected:
    /** @brief Reads a single state member. Deduces interface and type from the member pointer. */
    template <class Interface, class T>
    T read_state_value(T Interface::State::* member) const
    {
        return ::velk::read_state_value<Interface>(obj_.get(), member);
    }

    /** @brief Writes a single state member. Deduces interface and type from the member pointer. */
    template <class Interface, class T>
    void write_state_value(T Interface::State::* member, const T& value)
    {
        ::velk::write_state_value<Interface>(obj_.get(), member, value);
    }

    /** @brief Calls fn(*interface) if the object implements I. Returns fn's result or default. */
    template <class I, class Fn>
    auto with(Fn&& fn) const -> std::invoke_result_t<Fn, I&>
    {
        static_assert(std::is_invocable_v<Fn, I&>,
                      "with<I>(fn): fn must be callable with I&");
        auto* p = as<I>();
        using R = decltype(fn(*p));
        if constexpr (std::is_void_v<R>) {
            if (p) {
                fn(*p);
            }
        } else {
            return p ? fn(*p) : R{};
        }
    }

    /** @brief Calls fn(*interface) if the object implements I, otherwise returns fallback. */
    template <class I, class Fn, class Default>
    auto with_or(Fn&& fn, Default&& fallback) const -> std::invoke_result_t<Fn, I&>
    {
        static_assert(std::is_invocable_v<Fn, I&>,
                      "with_or<I>(fn, fallback): fn must be callable with I&");
        auto* p = as<I>();
        return p ? fn(*p) : std::forward<Default>(fallback);
    }

private:
    IObject::Ptr obj_;
};

} // namespace velk

#endif // VELK_API_OBJECT_H

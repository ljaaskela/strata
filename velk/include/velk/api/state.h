#ifndef VELK_API_STATE_H
#define VELK_API_STATE_H

#include <velk/api/callback.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object.h>

namespace velk {
namespace detail {

/** @brief RAII read-only accessor to an interface's State struct. Null-safe. */
template <class T>
class StateReader
{
public:
    StateReader() = default;
    explicit StateReader(const typename T::State* state) : state_(state) {}
    explicit operator bool() const { return state_ != nullptr; }
    const typename T::State* operator->() const { return state_; }
    const typename T::State& operator*() const { return *state_; }
    StateReader(const StateReader&) = default;
    StateReader& operator=(const StateReader&) = default;

private:
    const typename T::State* state_{};
};

/** @brief RAII write accessor; fires notify on destruction. Null-safe. */
template <class T>
class StateWriter
{
public:
    StateWriter() = default;
    StateWriter(typename T::State* state, const IInterface* meta) : state_(state), meta_(meta) {}
    ~StateWriter()
    {
        if (state_ && meta_) {
            if (auto* m = interface_cast<IMetadata>(meta_)) {
                m->notify(MemberKind::Property, T::UID, Notification::Changed);
            }
        }
    }
    StateWriter(const StateWriter&) = delete;
    StateWriter& operator=(const StateWriter&) = delete;
    StateWriter(StateWriter&& o) noexcept : state_(o.state_), meta_(o.meta_)
    {
        o.state_ = nullptr;
        o.meta_ = nullptr;
    }
    StateWriter& operator=(StateWriter&&) = delete;

    explicit operator bool() const { return state_ != nullptr; }
    typename T::State* operator->() { return state_; }
    typename T::State& operator*() { return *state_; }

private:
    typename T::State* state_{};
    const IInterface* meta_{};
};

} // namespace detail

// -- IMetadata member template definitions --

template <class T>
detail::StateReader<T> IMetadata::read() const
{
    auto* state = const_cast<IMetadata*>(this)->template get_property_state<T>();
    return detail::StateReader<T>(state);
}

template <class T>
detail::StateWriter<T> IMetadata::write()
{
    auto* state = this->template get_property_state<T>();
    return detail::StateWriter<T>(state, state ? this : nullptr);
}

// -- Free functions --

/**
 * @brief Type-safe property state access. Returns a typed pointer to T::State.
 * @tparam T The interface type whose State struct to retrieve.
 * @param object The object whose property state to return.
 */
template <class T, class U>
typename T::State* get_property_state(U* object)
{
    auto state = interface_cast<IPropertyState>(object);
    return state ? state->template get_property_state<T>() : nullptr;
}

/** @brief Convenience free function: read-only access to T::State via IMetadata. */
template <class T, class U>
detail::StateReader<T> read_state(U* object)
{
    auto* meta = interface_cast<IMetadata>(object);
    return meta ? meta->template read<T>() : detail::StateReader<T>();
}

/** @brief Convenience free function: write access to T::State via IMetadata. */
template <class T, class U>
detail::StateWriter<T> write_state(U* object)
{
    auto* meta = interface_cast<IMetadata>(object);
    return meta ? meta->template write<T>() : detail::StateWriter<T>();
}

/**
 * @brief Writes to T::State via a callback, with optional deferral.
 *
 * When @p type is Immediate, the callback executes synchronously and on_changed fires when it returns.
 * When @p type is Deferred, the callback is queued and executed on the next update() call.
 * If the object is destroyed before update(), the queued callback is silently skipped.
 *
 * @tparam T The interface type whose State struct to write.
 * @param object The object to modify.
 * @param fn Callback receiving a mutable T::State reference.
 * @param type Immediate or Deferred.
 */
template <class T, class U, class Fn>
void write_state(U* object, Fn&& fn, InvokeType type = Immediate)
{
    auto* meta = interface_cast<IMetadata>(object);
    if (!meta) {
        return;
    }
    auto* state = meta->template get_property_state<T>();
    if (!state) {
        return;
    }
    if (type == Immediate) {
        fn(*state);
        meta->notify(MemberKind::Property, T::UID, Notification::Changed);
        return;
    }
    auto* obj = interface_cast<IObject>(object);
    if (!obj) {
        return;
    }
    IObject::WeakPtr weak(obj->get_self());
    Callback cb([weak, f = std::forward<Fn>(fn)](FnArgs) mutable -> ReturnValue {
        auto locked = weak.lock();
        if (!locked) {
            return ReturnValue::Fail;
        }
        auto* m = interface_cast<IMetadata>(locked);
        if (!m) {
            return ReturnValue::Fail;
        }
        auto* s = m->template get_property_state<T>();
        if (!s) {
            return ReturnValue::Fail;
        }
        f(*s);
        m->notify(MemberKind::Property, T::UID, Notification::Changed);
        return ReturnValue::Success;
    });
    DeferredTask task{cb, {}};
    instance().queue_deferred_tasks({&task, 1});
}

} // namespace velk

#endif // VELK_API_STATE_H

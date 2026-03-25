#ifndef VELK_API_STATE_H
#define VELK_API_STATE_H

#include <velk/api/callback.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object.h>

namespace velk {

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

/** @brief Convenience free function: read-only access to T::State from a shared_ptr. */
template <class T, class U>
detail::StateReader<T> read_state(const shared_ptr<U>& object)
{
    return read_state<T>(object.get());
}

/** @brief Reads a single member from an object's interface state. Returns default if null. */
template <class Interface, class T>
T read_state_value(const IInterface* object, T Interface::State::* member)
{
    auto r = read_state<Interface>(object);
    return r ? (*r).*member : T{};
}

/** @brief Reads a single member from an object's interface state (shared_ptr overload). */
template <class Interface, class T, class U>
T read_state_value(const shared_ptr<U>& object, T Interface::State::* member)
{
    return read_state_value<Interface>(object.get(), member);
}

/** @brief Convenience free function: write access to T::State via IMetadata. */
template <class T, class U>
detail::StateWriter<T> write_state(U* object)
{
    auto* meta = interface_cast<IMetadata>(object);
    return meta ? meta->template write<T>() : detail::StateWriter<T>();
}

/** @brief Convenience free function: write access to T::State from a shared_ptr. */
template <class T, class U>
detail::StateWriter<T> write_state(const shared_ptr<U>& object)
{
    return write_state<T>(object.get());
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
void write_state(U* object, Fn&& fn, InvokeType type = Auto)
{
    auto* meta = interface_cast<IMetadata>(object);
    auto* state = get_property_state<T>(meta);
    if (!state) {
        return;
    }
    if (type != Deferred) {
        fn(*state);
        meta->notify(MemberKind::Property, T::UID, Notification::Changed);
        return;
    }
    IObject::WeakPtr weak(get_self(object));
    if (!weak.lock()) {
        return;
    }
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

/** @brief Callback-based write access to T::State from a shared_ptr. */
template <class T, class U, class Fn>
void write_state(const shared_ptr<U>& object, Fn&& fn, InvokeType type = Auto)
{
    write_state<T>(object.get(), std::forward<Fn>(fn), type);
}

/** @brief Writes a single member of an object's interface state. Fires on_changed. */
template <class Interface, class T>
void write_state_value(IInterface* object, T Interface::State::*member, const T& value)
{
    write_state<Interface>(object, [member, &value](typename Interface::State& s) {
        s.*member = value;
    });
}

/** @brief Writes a single member of an object's interface state (shared_ptr overload). */
template <class Interface, class T, class U>
void write_state_value(const shared_ptr<U>& object, T Interface::State::*member, const T& value)
{
    write_state_value<Interface>(object.get(), member, value);
}

} // namespace velk

#endif // VELK_API_STATE_H

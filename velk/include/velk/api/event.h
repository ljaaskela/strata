#ifndef VELK_API_EVENT_H
#define VELK_API_EVENT_H

#include <velk/api/callback.h>
#include <velk/common.h>
#include <velk/interface/intf_event.h>
#include <velk/interface/intf_function.h>

#include <type_traits>

namespace velk {

/** @brief Lightweight wrapper around an existing IEvent pointer. */
class Event
{
public:
    /** @brief Default-constructs an empty Event (null-safe, all methods are no-ops). */
    Event() = default;

    /** @brief Wraps an existing IEvent pointer. */
    explicit Event(IEvent::Ptr existing) : event_(std::move(existing)) {}

    /** @brief Implicit conversion to IEvent::Ptr. */
    operator IEvent::Ptr() { return event_; }
    operator const IEvent::ConstPtr() const { return event_; }

    /** @brief Returns true if the underlying IEvent is valid. */
    operator bool() const { return event_.operator bool(); }

    /** @brief Adds a handler function for the event (null-safe). */
    ReturnValue add_handler(const IFunction::ConstPtr& fn, InvokeType type = Auto) const
    {
        return event_ ? event_->add_handler(fn, type) : ReturnValue::InvalidArgument;
    }

    /**
     * @brief Adds a lambda handler for the event (null-safe).
     *
     * Accepts a callable (lambda, functor, etc.) and automatically wraps it in a Callback
     * before registering it as a handler. The Callback is created inline and implicitly
     * converted to IFunction::ConstPtr.
     *
     * @tparam F Callable type (lambda, functor, function pointer)
     * @param callable The callable to wrap and register
     * @param type Immediate or Deferred handler execution
     *
     * @par Example
     * @code
     * Event evt = widget->on_clicked();
     * evt.add_handler([](FnArgs args) { std::cout << "clicked!\n"; return Success; });
     * evt.add_handler([](int x, float y) { std::cout << x + y << std::endl; });
     * @endcode
     */
    template <class F, detail::require<!std::is_convertible_v<F, const IFunction::ConstPtr&>> = 0>
    ReturnValue add_handler(F&& callable, InvokeType type = Auto) const
    {
        Callback cb(std::forward<F>(callable));
        return add_handler(cb, type);
    }

    /** @brief Removes an event handler function (null-safe). */
    ReturnValue remove_handler(const IFunction::ConstPtr& fn) const
    {
        return event_ ? event_->remove_handler(fn) : ReturnValue::InvalidArgument;
    }

    /** @brief Returns true if the event has any handlers (null-safe). */
    bool has_handlers() const { return event_ ? event_->has_handlers() : false; }

    /** @brief Invokes the event with no arguments (null-safe). */
    ReturnValue invoke(InvokeType type = Auto) const
    {
        if (!event_) {
            return ReturnValue::InvalidArgument;
        }
        event_->invoke({}, type);
        return ReturnValue::Success;
    }

    /** @brief Invokes the event with the given @p args (null-safe). */
    ReturnValue invoke(FnArgs args, InvokeType type = Auto) const
    {
        if (!event_) {
            return ReturnValue::InvalidArgument;
        }
        event_->invoke(args, type);
        return ReturnValue::Success;
    }

private:
    IEvent::Ptr event_;
};

/**
 * @brief RAII event subscription. Subscribes on construction, unsubscribes on destruction.
 *
 * Move-only. When moved-from, the source becomes inert (won't unsubscribe).
 *
 * @par Example
 * @code
 * velk::ScopedHandler sh(widget->on_clicked(), [](FnArgs) {
 *     // handle click
 *     return ReturnValue::Success;
 * });
 * // handler is active until sh goes out of scope
 * @endcode
 */
class ScopedHandler
{
public:
    ScopedHandler() = default;

    /** @brief Subscribes @p callable to @p evt. */
    template <class F>
    ScopedHandler(Event evt, F&& callable)
        : event_(std::move(evt))
    {
        Callback cb(std::forward<F>(callable));
        handler_ = static_cast<IFunction::ConstPtr>(cb);
        event_.add_handler(handler_);
    }

    ~ScopedHandler() { reset(); }

    ScopedHandler(const ScopedHandler&) = delete;
    ScopedHandler& operator=(const ScopedHandler&) = delete;

    ScopedHandler(ScopedHandler&& other) noexcept
        : event_(std::move(other.event_))
        , handler_(std::move(other.handler_))
    {
        other.handler_ = {};
    }

    ScopedHandler& operator=(ScopedHandler&& other) noexcept
    {
        if (this != &other) {
            reset();
            event_ = std::move(other.event_);
            handler_ = std::move(other.handler_);
            other.handler_ = {};
        }
        return *this;
    }

    /** @brief Unsubscribes early. Safe to call multiple times. */
    void reset()
    {
        if (handler_ && event_) {
            event_.remove_handler(handler_);
            handler_ = {};
        }
    }

    /** @brief Returns true if this guard holds an active subscription. */
    explicit operator bool() const { return handler_.operator bool(); }

private:
    Event event_{IEvent::Ptr{}};
    IFunction::ConstPtr handler_;
};

} // namespace velk

#endif // VELK_API_EVENT_H

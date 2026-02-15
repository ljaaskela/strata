#ifndef INTF_FUNCTION_H
#define INTF_FUNCTION_H

#include <common.h>
#include <interface/intf_any.h>
#include <interface/intf_interface.h>

namespace strata {

/** @brief Specifies whether an invocation should execute immediately or be deferred to update(). */
enum InvokeType : uint8_t { Immediate = 0, Deferred = 1 };

/** @brief Non-owning view of function arguments. */
struct FnArgs {
    const IAny* const* data = nullptr;
    size_t count = 0;

    const IAny* operator[](size_t i) const { return i < count ? data[i] : nullptr; }
    bool empty() const { return count == 0; }
};

/** @brief Interface for an invocable function object. */
class IFunction : public Interface<IFunction>
{
public:
    /** @brief Function pointer type for invoke callbacks. */
    using CallableFn = ReturnValue(FnArgs);
    /**
     * @brief Called to invoke the function.
     * @param args Call args as a non-owning view.
     * @param type Immediate executes now; Deferred queues for the next update() call.
     * @return Function return value.
     */
    virtual ReturnValue invoke(FnArgs args, InvokeType type = Immediate) const = 0;
};

/**
 * @brief Internal interface for configuring an IFunction's invoke callback.
 *
 * Supports two dispatch mechanisms:
 * - @c set_invoke_callback() for explicit function pointer callbacks (highest priority).
 * - @c bind() for trampoline-based virtual dispatch, used by STRATA_INTERFACE
 *   to route @c invoke() calls to @c fn_Name() virtual methods on the interface.
 */
class IFunctionInternal : public Interface<IFunctionInternal>
{
public:
    /** @brief Function pointer type for bound trampoline callbacks. */
    using BoundFn = ReturnValue(void* context, FnArgs);

    /** @brief Sets the callback that will be called when IFunction::invoke is called. */
    virtual void set_invoke_callback(IFunction::CallableFn *fn) = 0;

    /**
     * @brief Binds a context pointer and trampoline function for virtual dispatch.
     * @param context Pointer to the interface subobject (passed as first arg to fn).
     * @param fn Static trampoline that casts context and calls the virtual method.
     */
    virtual void bind(void* context, BoundFn* fn) = 0;
};

/**
 * @brief Invokes a function with null safety.
 * @param fn Function to invoke.
 * @param args Arguments for invocation.
 * @param type Immediate executes now; Deferred queues for the next update() call.
 */
[[maybe_unused]] static ReturnValue invoke_function(const IFunction::Ptr &fn, FnArgs args = {}, InvokeType type = Immediate)
{
    return fn ? fn->invoke(args, type) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
[[maybe_unused]] static ReturnValue invoke_function(const IFunction::ConstPtr &fn, FnArgs args = {}, InvokeType type = Immediate)
{
    return fn ? fn->invoke(args, type) : ReturnValue::INVALID_ARGUMENT;
}

/** @brief Invokes a function with a single IAny argument. */
[[maybe_unused]] static ReturnValue invoke_function(const IFunction::Ptr &fn, const IAny *arg, InvokeType type = Immediate)
{
    FnArgs args{&arg, 1};
    return fn ? fn->invoke(args, type) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
[[maybe_unused]] static ReturnValue invoke_function(const IFunction::ConstPtr &fn, const IAny *arg, InvokeType type = Immediate)
{
    FnArgs args{&arg, 1};
    return fn ? fn->invoke(args, type) : ReturnValue::INVALID_ARGUMENT;
}

} // namespace strata

#endif // INTF_FUNCTION_H

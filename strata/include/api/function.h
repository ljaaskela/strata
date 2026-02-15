#ifndef API_FUNCTION_H
#define API_FUNCTION_H

#include <api/any.h>
#include <api/function_context.h>
#include <api/strata.h>
#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_function.h>
#include <interface/intf_metadata.h>

#include <tuple>
#include <type_traits>
#include <utility>

namespace strata {

/** @brief Convenience wrapper that creates and owns an IFunction with a callback. */
class Function : public ext::ObjectCore<Function>
{
public:
    /** @brief Type alias for the native callback signature. */
    using CallbackFn = IFunction::CallableFn;

    Function() = delete;
    /** @brief Creates a Function backed by the given callback. */
    Function(CallbackFn *cb)
    {
        fn_ = instance().create<IFunction>(ClassId::Function);
        if (auto internal = interface_cast<IFunctionInternal>(fn_); internal && cb) {
            internal->set_invoke_callback(cb);
        }
    }

    /** @brief Implicit conversion to IFunction::Ptr. */
    operator IFunction::Ptr() { return fn_; }
    operator const IFunction::ConstPtr() const { return fn_; }

    /** @brief Invokes the function with no arguments.
     *  @param type Immediate executes now; Deferred queues for the next update() call. */
    ReturnValue invoke(InvokeType type = Immediate) const { return fn_->invoke({}, type); }
    /** @brief Invokes the function with the given @p args.
     *  @param args Arguments for invocation.
     *  @param type Immediate executes now; Deferred queues for the next update() call. */
    ReturnValue invoke(FnArgs args, InvokeType type = Immediate) const { return fn_->invoke(args, type); }

private:
    IFunction::Ptr fn_;
};

// Variadic invoke_function: IAny-convertible args

/**
 * @brief Invokes a function with multiple IAny-convertible arguments.
 * @param fn Function to invoke.
 * @param args Two or more IAny-convertible arguments.
 */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IFunction::Ptr& fn, const Args&... args)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(args)...};
    return fn ? fn->invoke(FnArgs{ptrs, sizeof...(Args)}) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IFunction::ConstPtr& fn, const Args&... args)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(args)...};
    return fn ? fn->invoke(FnArgs{ptrs, sizeof...(Args)}) : ReturnValue::INVALID_ARGUMENT;
}

/**
 * @brief Invokes a named function with multiple IAny-convertible arguments.
 * @param o The object to query for the function.
 * @param name Name of the function to query.
 * @param args Two or more IAny-convertible arguments.
 */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface* o, std::string_view name, const Args&... args)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(args)...};
    auto* meta = interface_cast<IMetadata>(o);
    return meta ? invoke_function(meta->get_function(name), FnArgs{ptrs, sizeof...(Args)}) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface::Ptr& o, std::string_view name, const Args&... args)
{
    return invoke_function(o.get(), name, args...);
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface::ConstPtr& o, std::string_view name, const Args&... args)
{
    return invoke_function(o.get(), name, args...);
}

// Variadic invoke_function: value args (auto-wrapped in Any<T>)

namespace detail {

template<class Tuple, size_t... Is>
ReturnValue invoke_with_any_tuple(const IFunction::Ptr& fn, Tuple& tup, std::index_sequence<Is...>)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(std::get<Is>(tup))...};
    return fn ? fn->invoke(FnArgs{ptrs, sizeof...(Is)}) : ReturnValue::INVALID_ARGUMENT;
}

template<class Tuple, size_t... Is>
ReturnValue invoke_with_any_tuple(const IFunction::ConstPtr& fn, Tuple& tup, std::index_sequence<Is...>)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(std::get<Is>(tup))...};
    return fn ? fn->invoke(FnArgs{ptrs, sizeof...(Is)}) : ReturnValue::INVALID_ARGUMENT;
}

template<class Tuple, size_t... Is>
FnArgs make_fn_args(Tuple& tup, const IAny** ptrs, std::index_sequence<Is...>)
{
    ((ptrs[Is] = static_cast<const IAny*>(std::get<Is>(tup))), ...);
    return {ptrs, sizeof...(Is)};
}

} // namespace detail

/**
 * @brief Invokes a function with multiple value arguments.
 *
 * Each argument is wrapped in Any<T> and passed as FnArgs.
 */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IFunction::Ptr& fn, const Args&... args)
{
    auto tup = std::make_tuple(Any<std::decay_t<Args>>(args)...);
    return detail::invoke_with_any_tuple(fn, tup, std::index_sequence_for<Args...>{});
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IFunction::ConstPtr& fn, const Args&... args)
{
    auto tup = std::make_tuple(Any<std::decay_t<Args>>(args)...);
    return detail::invoke_with_any_tuple(fn, tup, std::index_sequence_for<Args...>{});
}

/** @brief Invokes a named function with multiple value arguments. */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface* o, std::string_view name, const Args&... args)
{
    auto tup = std::make_tuple(Any<std::decay_t<Args>>(args)...);
    const IAny* ptrs[sizeof...(Args)];
    auto fnArgs = detail::make_fn_args(tup, ptrs, std::index_sequence_for<Args...>{});
    auto* meta = interface_cast<IMetadata>(o);
    return meta ? invoke_function(meta->get_function(name), fnArgs) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface::Ptr& o, std::string_view name, const Args&... args)
{
    return invoke_function(o.get(), name, args...);
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface::ConstPtr& o, std::string_view name, const Args&... args)
{
    return invoke_function(o.get(), name, args...);
}

} // namespace strata

#endif // API_FUNCTION_H

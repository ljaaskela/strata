#ifndef INTF_FUNCTION_H
#define INTF_FUNCTION_H

#include <common.h>
#include <interface/intf_any.h>
#include <interface/intf_interface.h>

namespace strata {

/** @brief Interface for an invocable function object. */
class IFunction : public Interface<IFunction>
{
public:
    /** @brief Function pointer type for invoke callbacks. */
    using CallableFn = ReturnValue(const IAny *);
    /**
     * @brief Called to invoke the function
     * @param args Call args. Actual content dependent on the function implementation.
     * @return Function return value.
     */
    virtual ReturnValue invoke(const IAny *args) const = 0;
};

/** @brief Internal interface for configuring an IFunction's invoke callback. */
class IFunctionInternal : public Interface<IFunctionInternal>
{
public:
    /** @brief Sets the callback that will be called when IFunction::invoke is called. */
    virtual void set_invoke_callback(IFunction::CallableFn *fn) = 0;
};

} // namespace strata

#endif // INTF_FUNCTION_H

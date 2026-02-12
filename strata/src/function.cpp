#include "function.h"

namespace strata {

ReturnValue FunctionImpl::invoke(const IAny *args) const
{
    bool has_primary = fn_ || bound_fn_;
    ReturnValue result = ReturnValue::NOTHING_TO_DO;
    if (fn_) {
        result = fn_(args);
    } else if (bound_fn_) {
        result = bound_fn_(bound_context_, args);
    }
    for (const auto &h : handlers_) {
        h->invoke(args);
    }
    if (has_primary) {
        return result;
    }
    return handlers_.empty() ? ReturnValue::NOTHING_TO_DO : ReturnValue::SUCCESS;
}

void FunctionImpl::set_invoke_callback(IFunction::CallableFn *fn)
{
    fn_ = fn;
}

void FunctionImpl::bind(void* context, IFunctionInternal::BoundFn* fn)
{
    bound_context_ = context;
    bound_fn_ = fn;
}

const IFunction::ConstPtr FunctionImpl::get_invocable() const
{
    return get_self<IFunction>();
}

ReturnValue FunctionImpl::add_handler(const IFunction::ConstPtr &fn) const
{
    if (!fn) {
        return ReturnValue::INVALID_ARGUMENT;
    }
    for (const auto &h : handlers_) {
        if (h == fn) {
            return ReturnValue::NOTHING_TO_DO;
        }
    }
    handlers_.push_back(fn);
    return ReturnValue::SUCCESS;
}

ReturnValue FunctionImpl::remove_handler(const IFunction::ConstPtr &fn) const
{
    auto pos = 0;
    for (const auto &h : handlers_) {
        if (h == fn) {
            handlers_.erase(handlers_.begin() + pos);
            return ReturnValue::SUCCESS;
        }
        pos++;
    }
    return ReturnValue::NOTHING_TO_DO;
}

} // namespace strata

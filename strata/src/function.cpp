#include "function.h"

namespace strata {

ReturnValue FunctionImpl::invoke(const IAny *args) const
{
    if (fn_) {
        return fn_(args);
    }
    return ReturnValue::NOTHING_TO_DO;
}

void FunctionImpl::set_invoke_callback(IFunction::CallableFn *fn)
{
    fn_ = fn;
}

} // namespace strata

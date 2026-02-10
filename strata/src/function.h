#ifndef FUNCTION_H
#define FUNCTION_H

#include "strata_impl.h"
#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_function.h>
#include <interface/types.h>

namespace strata {

class FunctionImpl final : public CoreObject<FunctionImpl, IFunction, IFunctionInternal>
{
public:
    FunctionImpl() = default;
    ReturnValue Invoke(const IAny *args) const override;
    void SetInvokeCallback(IFunction::CallableFn *fn) override;

private:
    IFunction::CallableFn *fn_{};
};

} // namespace strata

#endif // FUNCTION_H

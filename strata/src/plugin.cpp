#include "registry.h"

#include "strata_export.h"

namespace strata {

STRATA_EXPORT IRegistry &GetRegistry()
{
    static Registry r;
    return *(r.GetInterface<IRegistry>());
}

} // namespace strata

#include "strata_impl.h"

#include "strata_export.h"

namespace strata {

STRATA_EXPORT IStrata &Strata()
{
    static StrataImpl r;
    return *(r.GetInterface<IStrata>());
}

} // namespace strata

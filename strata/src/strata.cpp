#include "strata_impl.h"

#include "strata_export.h"

namespace strata {

STRATA_EXPORT IStrata &instance()
{
    static StrataImpl r;
    return *(r.get_interface<IStrata>());
}

} // namespace strata

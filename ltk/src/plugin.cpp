#include "registry.h"

#define LTK_EXPORTS

#include "ltk_export.h"

LTK_EXPORT IRegistry &GetRegistry()
{
    static Registry r;
    return *(r.GetInterface<IRegistry>());
}

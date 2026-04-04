#ifndef VELK_GENERIC_OBJECT_H
#define VELK_GENERIC_OBJECT_H

#include <velk/ext/object.h>
#include <velk/interface/types.h>

namespace velk::impl {

/// Bare object with metadata support for holding dynamic properties.
class GenericObject : public ext::Object<GenericObject>
{
public:
    VELK_CLASS_UID(ClassId::Object, "Object");
};

} // namespace velk::impl

#endif // VELK_GENERIC_OBJECT_H

#ifndef VELK_INTF_OBJECT_REF_H
#define VELK_INTF_OBJECT_REF_H

#include <velk/interface/intf_any.h>

namespace velk {

/**
 * @brief Object reference value container.
 *
 * Extends IAny to store a reference to another IObject. Supports both owning
 * (strong) and non-owning (weak) modes, and optional interface constraint
 * validation on set.
 *
 * When used as a property type via VELK_INTERFACE, get_data/set_data operate
 * on IObject::Ptr values (type_uid = type_uid<IObject::Ptr>()).
 */
class IObjectRef : public Interface<IObjectRef, IAny>
{
public:
    /** @brief Returns the referenced object, or nullptr if empty/expired. */
    virtual IObject::Ptr get_object() const = 0;
    /** @brief Sets the referenced object. Validates constraint if set. */
    virtual ReturnValue set_object(const IObject::Ptr& obj) = 0;
    /** @brief Returns true if this ref holds a strong (owning) reference. */
    virtual bool is_owning() const = 0;
    /** @brief Sets the owning mode. Fails if switching to owning and the weak ref has expired. */
    virtual ReturnValue set_owning(bool owning) = 0;
    /** @brief Returns the constraint UID, or empty Uid if unconstrained. */
    virtual Uid constraint_uid() const = 0;
    /** @brief Sets the interface constraint. set_object() will reject objects that don't implement it. */
    virtual ReturnValue set_constraint(Uid uid) = 0;
};

} // namespace velk

#endif // VELK_INTF_OBJECT_REF_H

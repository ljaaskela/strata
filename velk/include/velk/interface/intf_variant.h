#ifndef VELK_INTF_VARIANT_H
#define VELK_INTF_VARIANT_H

#include <velk/interface/intf_any.h>

namespace velk {

/**
 * @brief Type-erased variant value container.
 *
 * Extends IAny with the ability to store any type and convert between
 * compatible types on read. Consumers can introspect the stored type
 * and query available conversions.
 */
class IVariant : public Interface<IVariant, IAny>
{
public:
    /** @brief Returns the UID of the currently stored type, or empty Uid if nothing is stored. */
    virtual Uid stored_type() const = 0;
    /** @brief Returns true if the stored value can be converted to the given type. */
    virtual bool can_convert_to(Uid type) const = 0;
};

} // namespace velk

#endif // VELK_INTF_VARIANT_H

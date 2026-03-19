#ifndef VELK_INTF_STORAGE_OWNED_H
#define VELK_INTF_STORAGE_OWNED_H

#include <velk/interface/intf_interface.h>

namespace velk {

class IObjectStorage; // forward declaration (raw pointer only, no include needed)

/** @brief Interface for items owned by an ObjectStorage container. */
class IStorageOwned : public Interface<IStorageOwned>
{
public:
    /** @brief Returns the ObjectStorage that owns this item, or nullptr for standalone items. */
    virtual IObjectStorage* get_owner() const = 0;
    /** @brief Returns this item's index within its owning ObjectStorage's member array. */
    virtual size_t get_storage_id() const = 0;
    /** @brief Sets the owning storage and member index. Called by ObjectStorage during creation. */
    virtual void set_owner(IObjectStorage* storage, size_t id) = 0;
};

} // namespace velk

#endif // VELK_INTF_STORAGE_OWNED_H

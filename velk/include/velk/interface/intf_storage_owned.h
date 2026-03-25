#ifndef VELK_INTF_STORAGE_OWNED_H
#define VELK_INTF_STORAGE_OWNED_H

#include <velk/interface/intf_interface.h>

namespace velk {

class IObjectStorage; // forward declaration (raw pointer only, no include needed)

/** @brief Interface for items owned by an ObjectStorage container. */
class IStorageOwned : public Interface<IStorageOwned>
{
public:
    static constexpr uint16_t InvalidIndex = uint16_t(-1);

    /** @brief Returns the ObjectStorage that owns this item, or nullptr for standalone items. */
    virtual IObjectStorage* get_owner() const = 0;
    /** @brief Returns this item's index within the static metadata array. */
    virtual uint16_t get_member_index() const = 0;
    /** @brief Returns this item's slot index within ObjectStorage's metadata vector. O(1) lookup. */
    virtual uint16_t get_storage_id() const = 0;
    /** @brief Sets the owning storage, member index, and slot index. Called by ObjectStorage. */
    virtual void set_owner(IObjectStorage* storage, uint16_t member_index, uint16_t storage_id) = 0;
};

} // namespace velk

#endif // VELK_INTF_STORAGE_OWNED_H

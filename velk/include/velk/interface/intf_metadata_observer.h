#ifndef VELK_INTF_METADATA_OBSERVER_H
#define VELK_INTF_METADATA_OBSERVER_H

#include <velk/interface/intf_interface.h>

namespace velk {

class IMetadata;

/**
 * @brief Observer interface for receiving object-level state change notifications.
 *
 * This callback may be invoked frequently (once per property set_value, once per write_state call).
 * Implementations should keep handling lightweight.
 */
class IMetadataObserver : public Interface<IMetadataObserver>
{
public:
    /**
     * @brief Called when state on the observed object changes.
     * @param name The property name that changed, or empty if the change came from write_state
     *        (where individual field changes are unknown).
     * @param owner The object's IMetadata, for lazily querying properties if needed.
     * @param interfaceId The UID of the interface whose state changed.
     */
    virtual void on_state_changed(string_view name, IMetadata& owner, Uid interfaceId) = 0;
};

} // namespace velk

#endif // VELK_INTF_METADATA_OBSERVER_H

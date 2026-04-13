#ifndef VELK_INTF_METADATA_OBSERVER_H
#define VELK_INTF_METADATA_OBSERVER_H

#include <velk/array_view.h>
#include <velk/interface/intf_interface.h>

#include <initializer_list>

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

/**
 * @brief A helper for checking if a state change notification matches a given interface and
 * any of the listed property names. Returns true when the name is empty (write_state changed
 * the whole state block) or matches one of the provided names.
 *
 * @par Example
 * @code
 * void MyTrait::on_state_changed(string_view name, IMetadata& owner, Uid interfaceId)
 * {
 *     constexpr string_view names[] = {"uri", "tint"};
 *     if (has_state_changed<IMyTrait>(interfaceId, name, names)) {
 *         // uri or state has changed or write_state<IMyTrait> has been used to modify the whole block
 *     }
 * }
 * @endcode
 */
template <typename T>
constexpr bool has_state_changed(Uid interfaceId, string_view name, array_view<const string_view> properties)
{
    if (interfaceId == T::UID) {
        if (name.empty()) {
            return true;
        }
        for (auto& prop : properties) {
            if (name == prop) {
                return true;
            }
        }
    }
    return false;
}

} // namespace velk

#endif // VELK_INTF_METADATA_OBSERVER_H

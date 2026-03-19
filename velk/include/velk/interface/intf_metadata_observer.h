#ifndef VELK_INTF_METADATA_OBSERVER_H
#define VELK_INTF_METADATA_OBSERVER_H

#include <velk/interface/intf_interface.h>
#include <velk/interface/intf_property.h>

namespace velk {

/** @brief Observer interface for receiving object-level property change notifications. */
class IMetadataObserver : public Interface<IMetadataObserver>
{
public:
    /** @brief Called when any property on the observed object changes. */
    virtual void on_property_changed(IProperty& property) = 0;
};

} // namespace velk

#endif // VELK_INTF_METADATA_OBSERVER_H

#ifndef VELK_INTF_IMPORTER_EXTENSION_H
#define VELK_INTF_IMPORTER_EXTENSION_H

#include <velk/interface/intf_interface.h>
#include <velk/interface/types.h>

namespace velk {

class IObjectStorage;

/**
 * @brief Extension point for importer plugins.
 *
 * Any plugin can register a class implementing this interface. At import time,
 * the importer queries ITypeRegistry for all classes implementing
 * IImporterExtension and dispatches each top-level collection to the extension
 * that handles its key.
 *
 * Definition only: no implementations live in velk.dll.
 *
 * Chain: IInterface -> IImporterExtension
 */
class IImporterExtension : public Interface<IImporterExtension>
{
public:
    /** @brief Returns the top-level collection key this extension handles (e.g. "animations"). */
    virtual string_view collection_key() const = 0;
};

} // namespace velk

#endif // VELK_INTF_IMPORTER_EXTENSION_H

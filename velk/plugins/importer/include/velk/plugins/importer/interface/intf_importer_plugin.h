#ifndef VELK_IMPORTER_INTF_IMPORTER_PLUGIN_H
#define VELK_IMPORTER_INTF_IMPORTER_PLUGIN_H

#include <velk/interface/intf_plugin.h>
#include <velk/interface/intf_store.h>
#include <velk/string.h>
#include <velk/vector.h>

namespace velk {

/** @brief Result of an import operation. */
struct ImportResult
{
    IStore::Ptr store;       ///< The imported store, or null if parsing failed entirely.
    vector<string> errors;   ///< Collected errors (unknown classes, bad properties, parse errors, etc.).
};

/** @brief Extended plugin interface for the importer. */
class IImporterPlugin : public Interface<IImporterPlugin>
{
public:
    /** @brief Parses a JSON string and returns a populated IStore with any errors. */
    virtual ImportResult import_from_json(string_view json) const = 0;
    /**
     * @brief Registers a custom class name alias for use in JSON class fields.
     *
     * Not needed for normal use. The importer resolves class names automatically
     * via the type registry (UUID strings and ClassInfo::name). This method exists
     * for application-level code that drives imports directly and needs to map
     * custom names (e.g. "myapp.Widget") to class UIDs that have different
     * registered names.
     */
    virtual void register_class_alias(string_view alias, Uid class_uid) = 0;
};

} // namespace velk

#endif // VELK_IMPORTER_INTF_IMPORTER_PLUGIN_H

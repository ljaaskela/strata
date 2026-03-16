#ifndef VELK_INTF_IMPORTER_EXTENSION_H
#define VELK_INTF_IMPORTER_EXTENSION_H

#include <velk/interface/intf_interface.h>
#include <velk/interface/intf_store.h>
#include <velk/interface/types.h>

#include <cstddef>
#include <cstdint>

namespace velk {

/**
 * @brief Format-neutral read-only data tree for importer extensions.
 *
 * Defined in velk core so that extensions have no dependency on the importer
 * plugin. The importer wraps its internal JSON nodes into this interface.
 *
 * Uses the null object pattern: find() on a missing key and at() out of bounds
 * return a static null node. The null node returns zero/empty for all accessors
 * and returns itself for find/at, making chaining safe without null checks.
 */
class IImportData
{
public:
    virtual ~IImportData() = default;

    enum class Kind : uint8_t { Null, Bool, Number, String, Array, Object };

    virtual Kind kind() const = 0;
    virtual bool as_bool() const = 0;
    virtual double as_number() const = 0;
    virtual string_view as_string() const = 0;

    /** @brief Array and object: number of elements/entries. */
    virtual size_t count() const = 0;

    /** @brief Array: indexed element. Object: value at index (insertion order). */
    virtual const IImportData* at(size_t index) const = 0;

    /** @brief Object: value for key. Returns static null node if missing. */
    virtual const IImportData* find(string_view key) const = 0;

    /** @brief Object: key name at index (for iteration). */
    virtual string_view key_at(size_t index) const = 0;

    bool is_null() const { return kind() == Kind::Null; }
};

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
    /** @brief Processes the data subtree for this extension's collection key. */
    virtual void process(const IImportData& data, IStore& store) const = 0;
};

} // namespace velk

#endif // VELK_INTF_IMPORTER_EXTENSION_H

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
 *
 * Chain: IInterface -> IImportData
 */
class IImportData : public Interface<IImportData>
{
public:
    /** @brief Type discriminator for data nodes. */
    enum class Kind : uint8_t { Null, Bool, Number, String, Array, Object };

    /** @brief Returns the type of this node. */
    virtual Kind kind() const = 0;
    /** @brief Returns true if this is a null node. */
    virtual bool is_null() const = 0;
    /** @brief Returns the boolean value, or false if not a Bool node. */
    virtual bool as_bool() const = 0;
    /** @brief Returns the numeric value, or 0.0 if not a Number node. */
    virtual double as_number() const = 0;
    /** @brief Returns the string value, or empty if not a String node. */
    virtual string_view as_string() const = 0;

    /** @brief Array and object: number of elements/entries. */
    virtual size_t count() const = 0;

    /** @brief Array: indexed element. Object: value at index (insertion order). */
    virtual const IImportData& at(size_t index) const = 0;

    /** @brief Object: value for key. Returns static null node if missing. */
    virtual const IImportData& find(string_view key) const = 0;

    /** @brief Object: key name at index (for iteration). */
    virtual string_view key_at(size_t index) const = 0;
};

/**
 * @brief Resolves object and property paths during import.
 *
 * Supports direct ids ("widget_1"), hierarchy paths ("/scene/root/child"),
 * and property paths ("widget_1.width", "/scene/root/child.width").
 * When the path contains a dot suffix, resolves the property from the
 * parent object's metadata. The returned IObject::Ptr can be cast to
 * IProperty via interface_pointer_cast when a property path was used.
 *
 * Chain: IInterface -> IImportResolver
 */
class IImportResolver : public Interface<IImportResolver>
{
public:
    /**
     * @brief Resolves a path to an object or property.
     * @param path Direct id ("w1"), hierarchy path ("/scene/root/child"),
     *             or property path ("w1.width"). Property paths return the
     *             IProperty wrapped as IObject::Ptr.
     * @return The resolved object, or nullptr if not found.
     */
    virtual IObject::Ptr resolve(string_view path) const = 0;
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
    virtual void process(const IImportData& data, IStore& store,
                         const IImportResolver& resolver) const = 0;
};

} // namespace velk

#endif // VELK_INTF_IMPORTER_EXTENSION_H

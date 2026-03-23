#ifndef VELK_INTF_RESOURCE_H
#define VELK_INTF_RESOURCE_H

#include <velk/interface/intf_interface.h>
#include <velk/interface/types.h>
#include <velk/string.h>
#include <velk/string_view.h>
#include <velk/vector.h>

#include <cstdint>

namespace velk {

/**
 * @brief Base interface for all resources accessed through the resource store.
 *
 * A resource represents an addressable item identified by a URI. Concrete
 * subtypes (e.g. IFile) provide type-specific access methods.
 *
 * Chain: IInterface -> IResource
 */
class IResource : public Interface<IResource>
{
public:
    /** @brief Returns the full URI of this resource (e.g. "file://C:/data/test.json"). */
    virtual string_view uri() const = 0;

    /** @brief Returns true if the resource exists. */
    virtual bool exists() const = 0;

    /** @brief Returns the size of the resource in bytes, or -1 on failure. */
    virtual int64_t size() const = 0;
};

/**
 * @brief File resource providing read access to file contents.
 *
 * Obtained by casting an IResource::Ptr returned from IResourceStore::get_resource()
 * via interface_cast<IFile>.
 *
 * Chain: IInterface -> IResource -> IFile
 */
class IFile : public Interface<IFile, IResource>
{
public:
    /** @brief Reads the entire file as binary bytes. */
    virtual ReturnValue read(vector<uint8_t>& out) const = 0;

    /** @brief Reads the entire file as UTF-8 text. */
    virtual ReturnValue read_text(string& out) const = 0;
};

} // namespace velk

#endif // VELK_INTF_RESOURCE_H

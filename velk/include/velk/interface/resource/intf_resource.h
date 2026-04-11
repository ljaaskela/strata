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

    /**
     * @brief Returns whether this resource is pinned in the resource store cache.
     *
     * Persistent resources stay in the decoded-resource cache even when no
     * external consumer holds a reference. Non-persistent resources are
     * evicted as soon as the last reference is dropped (the default).
     *
     * Only meaningful for resources produced via a decoder (the only ones
     * that get cached). Setting this on a protocol-direct resource has no
     * effect.
     */
    virtual bool is_persistent() const = 0;

    /**
     * @brief Sets the persistence flag.
     *
     * Takes effect on the next IResourceStore::get_resource call: pinning
     * upgrades the cache entry to a strong reference, unpinning drops the
     * strong reference (the resource then survives only as long as
     * external references keep it alive).
     */
    virtual void set_persistent(bool value) = 0;
};

/**
 * @brief File resource providing read and write access to file contents.
 *
 * Obtained by casting an IResource::Ptr returned from IResourceStore::get_resource()
 * via interface_cast<IFile>. Whether write operations succeed depends on the
 * backing protocol; FileProtocol-backed resources support both read and write.
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

    /**
     * @brief Writes binary bytes to the file, replacing any existing contents.
     *
     * Implementations should write atomically (temp file + rename) where
     * possible so that readers never observe a half-written file. Returns
     * Fail if the backing protocol does not support writing.
     */
    virtual ReturnValue write(const uint8_t* data, size_t size) = 0;

    /** @brief Writes UTF-8 text to the file, replacing any existing contents. */
    virtual ReturnValue write_text(string_view text) = 0;
};

} // namespace velk

#endif // VELK_INTF_RESOURCE_H

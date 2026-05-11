#ifndef VELK_INTF_RESOURCE_PROTOCOL_H
#define VELK_INTF_RESOURCE_PROTOCOL_H

#include <velk/interface/resource/intf_resource.h>
#include <velk/string_view.h>

namespace velk {

/**
 * @brief Protocol handler for URI-based resource access.
 *
 * Each protocol handles a specific URI scheme (e.g. "file", "http").
 * Implementations register via the type registry and are discovered
 * automatically by IResourceStore.
 *
 * Chain: IInterface -> IResourceProtocol
 */
class IResourceProtocol : public Interface<IResourceProtocol>
{
public:
    /** @brief Returns the URI scheme this protocol handles (e.g. "file", "http"). */
    virtual string_view scheme() const = 0;

    /**
     * @brief Resolves a path to a resource.
     * @param path The path portion of the URI (after "scheme://").
     * @return A resource handle, or nullptr on failure.
     */
    virtual IResource::Ptr resolve(string_view path) const = 0;
};

/**
 * @brief Internal configuration interface for resource protocols.
 *
 * Allows setting the scheme and base path on configurable protocols
 * like FileProtocol. Not all protocols support these operations.
 *
 * Chain: IInterface -> IResourceProtocol -> IResourceProtocolInternal
 */
class IResourceProtocolInternal : public Interface<IResourceProtocolInternal, IResourceProtocol>
{
public:
    /** @brief Sets the URI scheme this protocol handles. */
    virtual ReturnValue set_scheme(string_view scheme) = 0;

    /** @brief Returns the current base path. */
    virtual string_view base_path() const = 0;

    /** @brief Sets a base path prepended to all resolved paths. */
    virtual ReturnValue set_base_path(string_view base_path) = 0;
};

/**
 * @brief In-memory protocol for serving registered byte buffers as
 *        resources.
 *
 * Callers register bytes under a path (`add_file`); `resolve` on the
 * same path then returns an IFile reading those bytes. The protocol
 * takes ownership of the bytes via a held vector; the registration
 * lasts until `remove_file` is called or the protocol is destroyed.
 *
 * Typical use: a plugin that receives embedded resources (e.g. images
 * packed inside a .glb) registers them under synthetic
 * `memory://...` paths and feeds the resulting URIs through the
 * existing decoder pipeline (`image:memory://...`).
 *
 * Chain: IInterface -> IResourceProtocol -> IMemoryProtocol
 */
class IMemoryProtocol : public Interface<IMemoryProtocol, IResourceProtocol>
{
public:
    /// Registers @p bytes under @p path. Copies the bytes. Returns
    /// Success on insertion, NothingToDo if @p path already exists.
    virtual ReturnValue add_file(string_view path, const uint8_t* bytes, size_t size) = 0;

    /// Removes a previously-registered entry. Returns NothingToDo if
    /// the path was not present.
    virtual ReturnValue remove_file(string_view path) = 0;
};

} // namespace velk

#endif // VELK_INTF_RESOURCE_PROTOCOL_H

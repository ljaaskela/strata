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

} // namespace velk

#endif // VELK_INTF_RESOURCE_PROTOCOL_H

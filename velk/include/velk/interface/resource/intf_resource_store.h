#ifndef VELK_INTF_RESOURCE_STORE_H
#define VELK_INTF_RESOURCE_STORE_H

#include <velk/interface/resource/intf_resource.h>
#include <velk/interface/resource/intf_resource_protocol.h>
#include <velk/string_view.h>

namespace velk {

/**
 * @brief Core resource access service accessible via instance().resource_store().
 *
 * Routes URIs to registered protocol handlers and returns typed resource handles.
 * Protocol handlers are discovered automatically from the type registry and can
 * also be registered explicitly via register_protocol().
 *
 * All URIs must include an explicit scheme prefix (e.g. "file://path").
 *
 * Chain: IInterface -> IResourceStore
 */
class IResourceStore : public Interface<IResourceStore>
{
public:
    /**
     * @brief Resolves a URI to a resource handle.
     * @param uri Full URI with scheme prefix (e.g. "file://C:/data/test.json").
     * @return A resource handle, or nullptr if the scheme is unknown or resolution fails.
     */
    virtual IResource::Ptr get_resource(string_view uri) const = 0;

    /**
     * @brief Registers a protocol handler.
     * @param protocol The protocol to register. Its scheme() determines which URIs it handles.
     */
    virtual ReturnValue register_protocol(const IResourceProtocol::Ptr& protocol) = 0;

    /**
     * @brief Unregisters a previously registered protocol handler.
     * @param protocol The protocol to unregister.
     */
    virtual ReturnValue unregister_protocol(const IResourceProtocol::Ptr& protocol) = 0;

    /**
     * @brief Finds a registered protocol handler by scheme.
     * @param scheme The URI scheme to look up (e.g. "file", "app").
     * @return The protocol handler, or nullptr if not found.
     */
    virtual IResourceProtocol::Ptr find_protocol(string_view scheme) const = 0;

    /**
     * @brief Resolves a URI and casts the result to the specified resource type.
     * @tparam T The target resource interface (e.g. IFile).
     * @return A typed pointer, or nullptr if the scheme is unknown or the cast fails.
     */
    template <class T>
    typename T::Ptr get_resource(string_view uri) const
    {
        return interface_pointer_cast<T>(get_resource(uri));
    }
};

} // namespace velk

#endif // VELK_INTF_RESOURCE_STORE_H

#ifndef VELK_SRC_RESOURCE_STORE_H
#define VELK_SRC_RESOURCE_STORE_H

#include <velk/ext/interface_dispatch.h>
#include <velk/interface/resource/intf_resource_store.h>

namespace velk {

/**
 * @brief Core IResourceStore implementation.
 *
 * Routes URIs to registered protocol handlers. Protocols are registered
 * explicitly via register_protocol().
 */
class ResourceStore final : public ext::InterfaceDispatch<IResourceStore>
{
public:
    IResource::Ptr get_resource(string_view uri) const override;
    ReturnValue register_protocol(const IResourceProtocol::Ptr& protocol) override;
    ReturnValue unregister_protocol(const IResourceProtocol::Ptr& protocol) override;
    IResourceProtocol::Ptr find_protocol(string_view scheme) const override;

private:
    /**
     * @brief Parses "scheme://path" and returns the matching protocol handler.
     * @param uri Full URI with scheme prefix.
     * @param out_path Receives the path portion (after "scheme://").
     * @return The matching protocol, or nullptr if the scheme is unknown.
     */
    const IResourceProtocol* resolve(string_view uri, string_view& out_path) const;

    struct ProtocolEntry
    {
        string scheme;
        IResourceProtocol::Ptr protocol;
    };
    vector<ProtocolEntry> protocols_;
};

} // namespace velk

#endif // VELK_SRC_RESOURCE_STORE_H

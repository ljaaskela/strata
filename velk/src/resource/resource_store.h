#ifndef VELK_SRC_RESOURCE_STORE_H
#define VELK_SRC_RESOURCE_STORE_H

#include <velk/ext/interface_dispatch.h>
#include <velk/interface/resource/intf_resource_store.h>

namespace velk {

/**
 * @brief Core IResourceStore implementation.
 *
 * Routes URIs to registered protocol handlers and decoders. Protocols and
 * decoders are registered explicitly via register_protocol() and
 * register_decoder().
 *
 * Decoder URIs have the form "name:inner_uri" (e.g. "image:app://logo.png").
 * Decoded results are deduplicated by full URI via a weak-ref cache.
 */
class ResourceStore final : public ext::InterfaceDispatch<IResourceStore>
{
public:
    IResource::Ptr get_resource(string_view uri) const override;
    ReturnValue register_protocol(const IResourceProtocol::Ptr& protocol) override;
    ReturnValue unregister_protocol(const IResourceProtocol::Ptr& protocol) override;
    IResourceProtocol::Ptr find_protocol(string_view scheme) const override;

    ReturnValue register_decoder(const IResourceDecoder::Ptr& decoder) override;
    ReturnValue unregister_decoder(const IResourceDecoder::Ptr& decoder) override;
    IResourceDecoder::Ptr find_decoder(string_view name) const override;

private:
    /**
     * @brief Parses "scheme://path" and returns the matching protocol handler.
     * @param uri Full URI with scheme prefix.
     * @param out_path Receives the path portion (after "scheme://").
     * @return The matching protocol, or nullptr if the scheme is unknown.
     */
    const IResourceProtocol* resolve(string_view uri, string_view& out_path) const;

    /**
     * @brief Looks up a cached decoded resource by full URI.
     * @return The cached resource, or nullptr if not present or expired.
     */
    IResource::Ptr cache_lookup(string_view uri) const;

    /** @brief Stores a decoded resource in the cache, keyed by full URI. */
    void cache_store(string_view uri, const IResource::Ptr& resource) const;

    struct ProtocolEntry
    {
        string scheme;
        IResourceProtocol::Ptr protocol;
    };
    vector<ProtocolEntry> protocols_;

    struct DecoderEntry
    {
        string name;
        IResourceDecoder::Ptr decoder;
    };
    vector<DecoderEntry> decoders_;

    struct CacheEntry
    {
        string uri;
        weak_ptr<IResource> weak;
        IResource::Ptr pinned; ///< Strong ref iff resource is persistent.
    };
    mutable vector<CacheEntry> decoded_cache_;
};

} // namespace velk

#endif // VELK_SRC_RESOURCE_STORE_H

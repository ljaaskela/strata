#include "resource_store.h"

namespace velk {

ReturnValue ResourceStore::register_protocol(const IResourceProtocol::Ptr& protocol)
{
    if (!protocol) {
        return InvalidArgument;
    }
    auto s = protocol->scheme();
    if (s.empty()) {
        return InvalidArgument;
    }

    // Replace existing protocol for same scheme.
    for (auto& entry : protocols_) {
        if (string_view(entry.scheme) == s) {
            entry.protocol = protocol;
            return Success;
        }
    }

    protocols_.push_back(ProtocolEntry{string(s), protocol});
    return Success;
}

ReturnValue ResourceStore::unregister_protocol(const IResourceProtocol::Ptr& protocol)
{
    if (!protocol) {
        return InvalidArgument;
    }
    for (size_t i = 0; i < protocols_.size(); ++i) {
        if (protocols_[i].protocol.get() == protocol.get()) {
            protocols_.erase(protocols_.begin() + i);
            return Success;
        }
    }
    return NothingToDo;
}

IResourceProtocol::Ptr ResourceStore::find_protocol(string_view scheme) const
{
    for (auto& entry : protocols_) {
        if (string_view(entry.scheme) == scheme) {
            return entry.protocol;
        }
    }
    return nullptr;
}

const IResourceProtocol* ResourceStore::resolve(string_view uri, string_view& out_path) const
{
    auto sep = uri.find("://");
    if (sep == string_view::npos) {
        return nullptr;
    }

    auto scheme = uri.substr(0, sep);
    out_path = uri.substr(sep + 3);

    for (auto& entry : protocols_) {
        if (string_view(entry.scheme) == scheme) {
            return entry.protocol.get();
        }
    }

    return nullptr;
}

ReturnValue ResourceStore::register_decoder(const IResourceDecoder::Ptr& decoder)
{
    if (!decoder) {
        return InvalidArgument;
    }
    auto n = decoder->name();
    if (n.empty()) {
        return InvalidArgument;
    }

    for (auto& entry : decoders_) {
        if (string_view(entry.name) == n) {
            entry.decoder = decoder;
            return Success;
        }
    }

    decoders_.push_back(DecoderEntry{string(n), decoder});
    return Success;
}

ReturnValue ResourceStore::unregister_decoder(const IResourceDecoder::Ptr& decoder)
{
    if (!decoder) {
        return InvalidArgument;
    }
    for (size_t i = 0; i < decoders_.size(); ++i) {
        if (decoders_[i].decoder.get() == decoder.get()) {
            decoders_.erase(decoders_.begin() + i);
            return Success;
        }
    }
    return NothingToDo;
}

IResourceDecoder::Ptr ResourceStore::find_decoder(string_view name) const
{
    for (auto& entry : decoders_) {
        if (string_view(entry.name) == name) {
            return entry.decoder;
        }
    }
    return nullptr;
}

IResource::Ptr ResourceStore::cache_lookup(string_view uri) const
{
    // Reconcile every entry: prune dead slots, sync the pinned slot to the
    // current is_persistent() flag on each live resource.
    IResource::Ptr found;
    for (size_t i = 0; i < decoded_cache_.size();) {
        auto& e = decoded_cache_[i];
        auto live = e.weak.lock();
        if (!live && !e.pinned) {
            decoded_cache_.erase(decoded_cache_.begin() + i);
            continue;
        }
        if (live) {
            if (live->is_persistent()) {
                if (!e.pinned) {
                    e.pinned = live;
                }
            } else if (e.pinned) {
                e.pinned.reset();
            }
        }
        if (string_view(e.uri) == uri && live) {
            found = live;
        }
        ++i;
    }
    return found;
}

void ResourceStore::cache_store(string_view uri, const IResource::Ptr& resource) const
{
    for (auto& e : decoded_cache_) {
        if (string_view(e.uri) == uri) {
            e.weak = resource;
            e.pinned = resource->is_persistent() ? resource : IResource::Ptr{};
            return;
        }
    }
    CacheEntry entry;
    entry.uri = string(uri);
    entry.weak = resource;
    if (resource->is_persistent()) {
        entry.pinned = resource;
    }
    decoded_cache_.push_back(std::move(entry));
}

IResource::Ptr ResourceStore::get_resource(string_view uri) const
{
    // Decoder form: "name:inner_uri" where the leading token is followed by
    // ':' but NOT "://". The inner part must itself be a valid protocol URI.
    auto colon = uri.find(':');
    if (colon != string_view::npos) {
        bool is_protocol_uri = uri.size() > colon + 2
                            && uri[colon + 1] == '/'
                            && uri[colon + 2] == '/';
        if (!is_protocol_uri) {
            auto name = uri.substr(0, colon);
            auto inner = uri.substr(colon + 1);
            if (auto decoder = find_decoder(name)) {
                if (auto cached = cache_lookup(uri)) {
                    return cached;
                }
                auto inner_resource = get_resource(inner);
                if (!inner_resource) {
                    return nullptr;
                }
                auto decoded = decoder->decode(inner_resource);
                if (decoded) {
                    cache_store(uri, decoded);
                }
                return decoded;
            }
        }
    }

    string_view path;
    auto* proto = resolve(uri, path);
    if (!proto) {
        return nullptr;
    }
    return proto->resolve(path);
}

} // namespace velk

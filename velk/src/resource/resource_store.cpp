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

IResource::Ptr ResourceStore::get_resource(string_view uri) const
{
    string_view path;
    auto* proto = resolve(uri, path);
    if (!proto) {
        return nullptr;
    }
    return proto->resolve(path);
}

} // namespace velk

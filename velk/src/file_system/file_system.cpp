#include "file_system.h"

#include <velk/interface/intf_type_registry.h>

namespace velk {

FileSystem::FileSystem(const ITypeRegistry& registry) : registry_(registry) {}

void FileSystem::discover_protocols() const
{
    protocols_.clear();

    // Scan the type registry for all classes implementing IFileProtocol.
    vector<Uid> uids;
    registry_.for_each_class(&uids, [](void* ctx, const ClassInfo& info) -> bool {
        auto* out = static_cast<vector<Uid>*>(ctx);
        for (size_t i = 0; i < info.interfaces.size(); i++) {
            if (info.interfaces[i].uid == IFileProtocol::UID) {
                out->push_back(info.uid);
                break;
            }
        }
        return true;
    });

    for (auto uid : uids) {
        auto instance = registry_.create(uid);
        if (!instance) {
            continue;
        }
        auto* proto = interface_cast<IFileProtocol>(instance.get());
        if (!proto) {
            continue;
        }
        auto s = proto->scheme();
        if (s.empty()) {
            continue;
        }
        protocols_.push_back(ProtocolEntry{string(s), std::move(instance), proto});
    }
}

const IFileProtocol* FileSystem::resolve(string_view uri, string_view& out_path) const
{
    // Parse "scheme://path"
    auto sep = uri.find("://");
    if (sep == string_view::npos) {
        return nullptr;
    }

    auto scheme = uri.substr(0, sep);
    out_path = uri.substr(sep + 3);

    discover_protocols();

    for (auto& entry : protocols_) {
        if (string_view(entry.scheme) == scheme) {
            return entry.protocol;
        }
    }
    return nullptr;
}

ReturnValue FileSystem::read_file(string_view uri, vector<uint8_t>& out) const
{
    string_view path;
    auto* proto = resolve(uri, path);
    if (!proto) {
        return InvalidArgument;
    }
    return proto->read_file(path, out);
}

ReturnValue FileSystem::read_text_file(string_view uri, string& out) const
{
    string_view path;
    auto* proto = resolve(uri, path);
    if (!proto) {
        return InvalidArgument;
    }
    return proto->read_text_file(path, out);
}

bool FileSystem::file_exists(string_view uri) const
{
    string_view path;
    auto* proto = resolve(uri, path);
    if (!proto) {
        return false;
    }
    return proto->file_exists(path);
}

int64_t FileSystem::file_size(string_view uri) const
{
    string_view path;
    auto* proto = resolve(uri, path);
    if (!proto) {
        return -1;
    }
    return proto->file_size(path);
}

} // namespace velk

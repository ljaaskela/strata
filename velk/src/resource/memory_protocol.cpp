#include "memory_protocol.h"

#include <velk/ext/core_object.h>
#include <velk/interface/resource/intf_resource.h>

#include <cstring>

namespace velk {

namespace {

/**
 * @brief IFile view over a byte buffer owned by MemoryProtocol.
 *
 * Captures the bytes as a vector copy at resolve time so the resource
 * stays valid even if MemoryProtocol evicts the registration later.
 * Simpler than sharing state with the protocol; memory overhead is
 * comparable to any other IFile that reads into a buffer.
 */
class MemoryResource final : public ext::ObjectCore<MemoryResource, IFile>
{
public:
    void set_data(string_view uri, vector<uint8_t> bytes)
    {
        uri_ = string(uri);
        bytes_ = std::move(bytes);
    }

    string_view uri() const override { return uri_; }
    bool exists() const override { return true; }
    int64_t size() const override { return static_cast<int64_t>(bytes_.size()); }
    bool is_persistent() const override { return persistent_; }
    void set_persistent(bool value) override { persistent_ = value; }

    ReturnValue read(vector<uint8_t>& out) const override
    {
        out.resize(bytes_.size());
        if (!bytes_.empty()) {
            std::memcpy(out.data(), bytes_.data(), bytes_.size());
        }
        return Success;
    }

    ReturnValue read_text(string& out) const override
    {
        out = string(string_view(reinterpret_cast<const char*>(bytes_.data()), bytes_.size()));
        return Success;
    }

    ReturnValue write(const uint8_t*, size_t) override { return Fail; }
    ReturnValue write_text(string_view) override { return Fail; }

private:
    string uri_;
    vector<uint8_t> bytes_;
    bool persistent_{false};
};

} // namespace

IResource::Ptr MemoryProtocol::resolve(string_view path) const
{
    vector<uint8_t> copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = files_.find(string(path));
        if (it == files_.end()) {
            return nullptr;
        }
        copy = it->second;
    }

    auto obj = ext::make_object<MemoryResource>();
    auto* mr = static_cast<MemoryResource*>(obj.get());
    string full_uri = "memory://" + string(path);
    mr->set_data(full_uri, std::move(copy));
    return interface_pointer_cast<IResource>(obj);
}

ReturnValue MemoryProtocol::add_file(string_view path, const uint8_t* bytes, size_t size)
{
    if (path.empty() || (!bytes && size > 0)) {
        return InvalidArgument;
    }

    vector<uint8_t> copy;
    copy.resize(size);
    if (size > 0) {
        std::memcpy(copy.data(), bytes, size);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto [it, inserted] = files_.emplace(string(path), std::move(copy));
    return inserted ? Success : NothingToDo;
}

ReturnValue MemoryProtocol::remove_file(string_view path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = files_.find(string(path));
    if (it == files_.end()) {
        return NothingToDo;
    }
    files_.erase(it);
    return Success;
}

} // namespace velk

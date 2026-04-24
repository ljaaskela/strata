#ifndef VELK_SRC_MEMORY_PROTOCOL_H
#define VELK_SRC_MEMORY_PROTOCOL_H

#include <velk/ext/core_object.h>
#include <velk/interface/resource/intf_resource_protocol.h>
#include <velk/string.h>
#include <velk/vector.h>

#include <mutex>
#include <unordered_map>

namespace velk {

/**
 * @brief In-memory protocol handler.
 *
 * Serves registered byte buffers under `memory://<path>` URIs. See
 * IMemoryProtocol for the add/remove contract.
 */
class MemoryProtocol final : public ext::ObjectCore<MemoryProtocol, IMemoryProtocol>
{
public:
    VELK_CLASS_UID(ClassId::MemoryProtocol, "MemoryProtocol");

    // IResourceProtocol
    string_view scheme() const override { return "memory"; }
    IResource::Ptr resolve(string_view path) const override;

    // IMemoryProtocol
    ReturnValue add_file(string_view path, const uint8_t* bytes, size_t size) override;
    ReturnValue remove_file(string_view path) override;

private:
    mutable std::mutex mutex_;
    std::unordered_map<string, vector<uint8_t>> files_;
};

} // namespace velk

#endif // VELK_SRC_MEMORY_PROTOCOL_H

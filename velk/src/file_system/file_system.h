#ifndef VELK_SRC_FILE_SYSTEM_H
#define VELK_SRC_FILE_SYSTEM_H

#include <velk/ext/interface_dispatch.h>
#include <velk/interface/intf_file_protocol.h>
#include <velk/interface/intf_file_system.h>

namespace velk {

class ITypeRegistry;

/**
 * @brief Core IFileSystem implementation.
 *
 * Discovers IFileProtocol handlers from the type registry and dispatches
 * URI-based requests to the matching protocol by scheme.
 */
class FileSystem final : public ext::InterfaceDispatch<IFileSystem>
{
public:
    explicit FileSystem(const ITypeRegistry& registry);

    ReturnValue read_file(string_view uri, vector<uint8_t>& out) const override;
    ReturnValue read_text_file(string_view uri, string& out) const override;
    bool file_exists(string_view uri) const override;
    int64_t file_size(string_view uri) const override;

private:
    /** @brief Discovers all IFileProtocol implementations from the type registry. */
    void discover_protocols() const;

    /**
     * @brief Parses "scheme://path" and returns the matching protocol handler.
     * @param uri Full URI with scheme prefix.
     * @param out_path Receives the path portion (after "scheme://").
     * @return The matching protocol, or nullptr if the scheme is unknown.
     */
    const IFileProtocol* resolve(string_view uri, string_view& out_path) const;

    const ITypeRegistry& registry_;

    struct ProtocolEntry
    {
        string scheme;
        IInterface::Ptr instance;
        IFileProtocol* protocol; ///< Raw pointer into instance.
    };
    mutable vector<ProtocolEntry> protocols_;
};

} // namespace velk

#endif // VELK_SRC_FILE_SYSTEM_H

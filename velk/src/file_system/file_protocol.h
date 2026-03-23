#ifndef VELK_SRC_FILE_PROTOCOL_H
#define VELK_SRC_FILE_PROTOCOL_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_file_protocol.h>

namespace velk {

/**
 * @brief Built-in "file" protocol handler for local filesystem access.
 *
 * Handles the "file://" URI scheme. Uses C-style file I/O internally
 * (no exceptions required).
 */
class FileProtocol final : public ext::ObjectCore<FileProtocol, IFileProtocol>
{
public:
    VELK_CLASS_UID("f17e5000-0001-4000-8000-000000000001", "FileProtocol");

    string_view scheme() const override;
    ReturnValue read_file(string_view path, vector<uint8_t>& out) const override;
    ReturnValue read_text_file(string_view path, string& out) const override;
    bool file_exists(string_view path) const override;
    int64_t file_size(string_view path) const override;
};

} // namespace velk

#endif // VELK_SRC_FILE_PROTOCOL_H

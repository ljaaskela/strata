#ifndef VELK_SRC_FILE_PROTOCOL_H
#define VELK_SRC_FILE_PROTOCOL_H

#include <velk/ext/core_object.h>
#include <velk/interface/resource/intf_resource_protocol.h>

namespace velk {

/**
 * @brief Local filesystem protocol handler.
 *
 * Handles a configurable URI scheme (default "file") and resolves paths
 * to FileResource instances. An optional base path is prepended to all
 * resolved paths.
 *
 * Multiple instances can be created with different schemes and base paths
 * (e.g. "file" with no base, "app" with the working directory).
 */
class FileProtocol final : public ext::ObjectCore<FileProtocol, IResourceProtocolInternal>
{
public:
    VELK_CLASS_UID(ClassId::FileProtocol, "FileProtocol");

    string_view scheme() const override;
    IResource::Ptr resolve(string_view path) const override;
    ReturnValue set_scheme(string_view scheme) override;
    string_view base_path() const override;
    ReturnValue set_base_path(string_view base_path) override;

private:
    string scheme_{"file"};
    string base_path_;
};

} // namespace velk

#endif // VELK_SRC_FILE_PROTOCOL_H

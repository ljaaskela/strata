#ifndef VELK_SRC_FILE_RESOURCE_H
#define VELK_SRC_FILE_RESOURCE_H

#include <velk/ext/core_object.h>
#include <velk/interface/resource/intf_resource.h>

namespace velk {

/**
 * @brief File resource backed by the local filesystem.
 *
 * Created by FileProtocol::resolve(). Holds a resolved filesystem path
 * and provides read access via the IFile interface.
 */
class FileResource final : public ext::ObjectCore<FileResource, IFile>
{
public:
    void set_path(string_view path, string_view uri);

    string_view uri() const override;
    bool exists() const override;
    int64_t size() const override;
    bool is_persistent() const override { return persistent_; }
    void set_persistent(bool value) override { persistent_ = value; }
    ReturnValue read(vector<uint8_t>& out) const override;
    ReturnValue read_text(string& out) const override;

private:
    string path_; ///< Resolved filesystem path.
    string uri_;  ///< Full URI (e.g. "file://C:/data/test.json").
    bool persistent_{false};
};

} // namespace velk

#endif // VELK_SRC_FILE_RESOURCE_H

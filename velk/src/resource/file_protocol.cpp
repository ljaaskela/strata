#include "file_protocol.h"
#include "file_resource.h"

#include <velk/ext/core_object.h>

namespace velk {

string_view FileProtocol::scheme() const
{
    return scheme_;
}

ReturnValue FileProtocol::set_scheme(string_view scheme)
{
    if (scheme.empty()) {
        return InvalidArgument;
    }
    scheme_ = string(scheme);
    return Success;
}

ReturnValue FileProtocol::set_base_path(string_view base_path)
{
    base_path_ = string(base_path);
    return Success;
}

IResource::Ptr FileProtocol::resolve(string_view path) const
{
    auto obj = ext::make_object<FileResource>();
    auto* fr = static_cast<FileResource*>(obj.get());

    string resolved;
    if (!base_path_.empty()) {
        resolved = base_path_ + string(path);
    } else {
        resolved = string(path);
    }

    string full_uri = string(scheme_) + "://" + string(path);
    fr->set_path(resolved, full_uri);

    return interface_pointer_cast<IResource>(obj);
}

} // namespace velk

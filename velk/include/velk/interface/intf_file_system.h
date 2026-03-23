#ifndef VELK_INTF_FILE_SYSTEM_H
#define VELK_INTF_FILE_SYSTEM_H

#include <velk/interface/intf_interface.h>
#include <velk/interface/types.h>
#include <velk/string.h>
#include <velk/string_view.h>
#include <velk/vector.h>

#include <cstdint>

namespace velk {

/**
 * @brief Core file system service accessible via instance().file_system().
 *
 * Parses the scheme from URIs (e.g. "file://", "assets://") and dispatches
 * to the appropriate IFileProtocol handler. All URIs must include an explicit
 * scheme prefix.
 *
 * Protocol handlers are discovered automatically from the type registry by
 * scanning for classes that implement IFileProtocol.
 *
 * Chain: IInterface -> IFileSystem
 */
class IFileSystem : public Interface<IFileSystem>
{
public:
    /** @brief Reads the entire resource as binary bytes. URI must include a scheme. */
    virtual ReturnValue read_file(string_view uri, vector<uint8_t>& out) const = 0;

    /** @brief Reads the entire resource as UTF-8 text. URI must include a scheme. */
    virtual ReturnValue read_text_file(string_view uri, string& out) const = 0;

    /** @brief Returns true if the resource exists. */
    virtual bool file_exists(string_view uri) const = 0;

    /** @brief Returns the size of the resource in bytes, or -1 on failure. */
    virtual int64_t file_size(string_view uri) const = 0;
};

} // namespace velk

#endif // VELK_INTF_FILE_SYSTEM_H

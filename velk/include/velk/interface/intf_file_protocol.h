#ifndef VELK_INTF_FILE_PROTOCOL_H
#define VELK_INTF_FILE_PROTOCOL_H

#include <velk/interface/intf_interface.h>
#include <velk/interface/types.h>
#include <velk/string.h>
#include <velk/string_view.h>
#include <velk/vector.h>

#include <cstdint>

namespace velk {

/**
 * @brief Protocol handler for URI-based file system access.
 *
 * Implementations register via the type registry and are discovered
 * automatically by IFileSystem. Each protocol handles a specific URI
 * scheme (e.g. "file", "assets").
 *
 * The path parameter in all methods receives the URI with the scheme
 * prefix stripped. For example, "file://C:/data/test.json" arrives
 * as "C:/data/test.json".
 *
 * Chain: IInterface -> IFileProtocol
 */
class IFileProtocol : public Interface<IFileProtocol>
{
public:
    /** @brief Returns the URI scheme this protocol handles (e.g. "file", "assets"). */
    virtual string_view scheme() const = 0;

    /** @brief Reads the entire resource as binary bytes. */
    virtual ReturnValue read_file(string_view path, vector<uint8_t>& out) const = 0;

    /** @brief Reads the entire resource as UTF-8 text. */
    virtual ReturnValue read_text_file(string_view path, string& out) const = 0;

    /** @brief Returns true if the resource exists. */
    virtual bool file_exists(string_view path) const = 0;

    /** @brief Returns the size of the resource in bytes, or -1 on failure. */
    virtual int64_t file_size(string_view path) const = 0;
};

} // namespace velk

#endif // VELK_INTF_FILE_PROTOCOL_H

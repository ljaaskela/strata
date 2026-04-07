#ifndef VELK_INTF_RESOURCE_DECODER_H
#define VELK_INTF_RESOURCE_DECODER_H

#include <velk/interface/resource/intf_resource.h>
#include <velk/string_view.h>

namespace velk {

/**
 * @brief Decodes one resource into another, typed resource.
 *
 * Decoders are distinct from protocols. A protocol turns a URI into bytes
 * (e.g. `file://`, `app://`). A decoder turns one resource into another
 * (e.g. raw PNG bytes into an `IImage` with a GPU texture).
 *
 * Decoders are addressed in URIs with the form `name:inner_uri`, where
 * `name` is the decoder's `name()` and `inner_uri` is a normal protocol
 * URI. For example: `image:app://logo.png` resolves the bytes via the
 * `app://` protocol, then runs the `image` decoder on the result.
 *
 * The resource store recognises this form, resolves the inner URI through
 * the normal protocol path, and applies the decoder. Decoded results are
 * deduplicated by full URI: a second call with the same `name:inner_uri`
 * returns the same `IResource::Ptr` while at least one consumer holds it.
 *
 * Decoders are registered explicitly with `IResourceStore::register_decoder()`,
 * typically in a plugin's `initialize()`.
 *
 * Chain: IInterface -> IResourceDecoder
 */
class IResourceDecoder : public Interface<IResourceDecoder>
{
public:
    /**
     * @brief Returns the decoder name, used as the leading token in
     *        `name:inner_uri` URIs (e.g. "image", "mesh", "audio").
     */
    virtual string_view name() const = 0;

    /**
     * @brief Decodes @p inner into a typed resource.
     * @param inner The resource produced by the inner protocol path
     *              (typically an IFile that the decoder reads bytes from).
     * @return A decoded resource, or nullptr if @p inner is fundamentally
     *         not of the expected input type. For partial decode failures
     *         (e.g. valid header but corrupt payload), prefer returning a
     *         non-null resource that exposes a failure status of its own,
     *         so consumers can degrade gracefully and the failure is cached.
     */
    virtual IResource::Ptr decode(const IResource::Ptr& inner) const = 0;
};

} // namespace velk

#endif // VELK_INTF_RESOURCE_DECODER_H

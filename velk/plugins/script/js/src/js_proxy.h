#ifndef VELK_JS_PROXY_H
#define VELK_JS_PROXY_H

#include <velk/interface/intf_object.h>

extern "C" {
#include <quickjs.h>
}

namespace velk {

/**
 * @brief Manages the QuickJS class for Velk object proxies.
 *
 * Each proxy wraps an IObject::Ptr. JS property access is routed to IMetadata
 * for property get/set, with fallback to events and functions.
 */
struct JsProxy
{
    /** @brief Registers the VelkObject JS class with the runtime. Must be called once per runtime. */
    static void register_class(JSRuntime* rt);

    /** @brief Creates a JS proxy object wrapping the given Velk object. */
    static JSValue create(JSContext* ctx, IObject::Ptr obj);

    /** @brief Extracts the IObject::Ptr from a proxy JSValue, or nullptr if not a proxy. */
    static IObject* get_object(JSValue val);

    /** @brief The QuickJS class ID for VelkObject proxies. */
    static JSClassID class_id;
};

} // namespace velk

#endif // VELK_JS_PROXY_H

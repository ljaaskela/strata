#ifndef VELK_JS_CONTEXT_H
#define VELK_JS_CONTEXT_H

#include "js_any.h"

#include <velk/interface/intf_object.h>
#include <velk/interface/intf_store.h>

extern "C" {
#include <quickjs.h>
}

#include <unordered_map>
#include <vector>

namespace velk {

/**
 * @brief Per-store JavaScript execution context.
 *
 * Owns a JSContext (created from the plugin's JSRuntime) and a cache of
 * object proxies. Objects in the store are lazily accessible as global
 * variables by their store ID.
 */
class JsStoreContext
{
public:
    JsStoreContext(JSRuntime* rt, IStore& store);
    ~JsStoreContext();

    JsStoreContext(const JsStoreContext&) = delete;
    JsStoreContext& operator=(const JsStoreContext&) = delete;

    /** @brief Returns the underlying JSContext. */
    JSContext* ctx() const { return ctx_; }

    /** @brief Returns the store this context is bound to. */
    IStore& store() const { return store_; }

    /**
     * @brief Returns or creates a JS proxy for the given Velk object.
     * Proxies are cached so the same object always maps to the same JSValue.
     */
    JSValue get_or_create_proxy(IObject::Ptr obj);

    /**
     * @brief Registers a Velk object as a global JS variable by name.
     */
    void register_global(const char* name, IObject::Ptr obj);

    /** @brief Tracks an IJsAny so it can be invalidated before the context is destroyed. */
    void track(const IJsAny::Ptr& any);

    /**
     * @brief Compiles a JS expression into a function.
     *
     * Wraps the expression as: (function() { return (expr); })
     * @param source The expression source text.
     * @param filename Optional filename for error reporting.
     * @return A JSValue function (caller must JS_FreeValue), or JS_EXCEPTION on error.
     */
    JSValue compile_expression(const char* source, const char* filename = "<expr>");

    /**
     * @brief Compiles a JS statement block into a function.
     *
     * Wraps as: (function() { body })
     * @param source The statement source text.
     * @param filename Optional filename for error reporting.
     * @return A JSValue function (caller must JS_FreeValue), or JS_EXCEPTION on error.
     */
    JSValue compile_handler(const char* source, const char* filename = "<handler>");

private:
    JSContext* ctx_;
    IStore& store_;
    std::unordered_map<IObject*, JSValue> proxy_cache_;
    std::vector<IJsAny::WeakPtr> tracked_;
};

} // namespace velk

#endif // VELK_JS_CONTEXT_H

#include "js_proxy.h"

#include "js_marshal.h"

#include <velk/interface/intf_metadata.h>

#include <cstring>

namespace velk {

JSClassID JsProxy::class_id = 0;

namespace {

/** @brief Opaque data attached to each proxy JSValue. */
struct ProxyData
{
    IObject::Ptr object;
};

void proxy_finalizer(JSRuntime*, JSValue val)
{
    auto* data = static_cast<ProxyData*>(JS_GetOpaque(val, JsProxy::class_id));
    delete data;
}

JSValue proxy_get(JSContext* ctx, JSValue obj, JSAtom atom, JSValue receiver)
{
    auto* data = static_cast<ProxyData*>(JS_GetOpaque(obj, JsProxy::class_id));
    if (!data || !data->object) {
        return JS_UNDEFINED;
    }

    const char* name = JS_AtomToCString(ctx, atom);
    if (!name) {
        return JS_UNDEFINED;
    }

    auto* meta = interface_cast<IMetadata>(data->object);
    if (!meta) {
        JS_FreeCString(ctx, name);
        return JS_UNDEFINED;
    }

    // Try property first (uses Resolve::Create to trigger lazy instantiation)
    auto prop = meta->get_property(string_view(name, std::strlen(name)), {}, Resolve::Create);
    if (prop) {
        auto value = prop->get_value();
        JS_FreeCString(ctx, name);
        return any_to_jsvalue(ctx, value.get());
    }

    JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
}

int proxy_set(JSContext* ctx, JSValue obj, JSAtom atom, JSValue value, JSValue receiver, int flags)
{
    auto* data = static_cast<ProxyData*>(JS_GetOpaque(obj, JsProxy::class_id));
    if (!data || !data->object) {
        return -1;
    }

    const char* name = JS_AtomToCString(ctx, atom);
    if (!name) {
        return -1;
    }

    auto* meta = interface_cast<IMetadata>(data->object);
    if (!meta) {
        JS_FreeCString(ctx, name);
        return -1;
    }

    auto prop = meta->get_property(string_view(name, std::strlen(name)), {}, Resolve::Create);
    if (!prop) {
        JS_FreeCString(ctx, name);
        return -1;
    }
    JS_FreeCString(ctx, name);

    // Get the property's type to know what to convert the JS value to
    auto current_value = prop->get_value();
    if (!current_value) {
        return -1;
    }
    auto types = current_value->get_compatible_types();
    if (types.empty()) {
        return -1;
    }

    auto any = jsvalue_to_any(ctx, value, types[0]);
    if (!any) {
        return -1;
    }

    prop->set_value(*any);
    return 0;
}

JSClassExoticMethods proxy_exotic = {
    nullptr,       // get_own_property
    nullptr,       // get_own_property_names
    nullptr,       // delete_property
    nullptr,       // define_own_property
    nullptr,       // has_property
    proxy_get,     // get_property
    proxy_set,     // set_property
};

JSClassDef proxy_class_def = {
    "VelkObject",
    proxy_finalizer,
    nullptr,       // gc_mark
    nullptr,       // call
    &proxy_exotic,
};

} // namespace

void JsProxy::register_class(JSRuntime* rt)
{
    JS_NewClassID(rt, &class_id);
    JS_NewClass(rt, class_id, &proxy_class_def);
}

JSValue JsProxy::create(JSContext* ctx, IObject::Ptr obj)
{
    JSValue js_obj = JS_NewObjectClass(ctx, static_cast<int>(class_id));
    if (JS_IsException(js_obj)) {
        return js_obj;
    }
    auto* data = new ProxyData{std::move(obj)};
    JS_SetOpaque(js_obj, data);
    return js_obj;
}

IObject* JsProxy::get_object(JSValue val)
{
    auto* data = static_cast<ProxyData*>(JS_GetOpaque(val, class_id));
    return data ? data->object.get() : nullptr;
}

} // namespace velk

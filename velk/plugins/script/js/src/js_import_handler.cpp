#include "js_import_handler.h"

#include "js_context.h"
#include "js_function.h"
#include "js_plugin.h"

#include <velk/api/binding.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_plugin_registry.h>
#include <velk/interface/intf_property.h>
#include <velk/plugins/script/js/plugin.h>

#include <cstring>

namespace velk {

namespace {

IProperty::Ptr resolve_property(const IImportResolver& resolver, string_view path)
{
    auto obj = resolver.resolve(path);
    return obj ? interface_pointer_cast<IProperty>(obj) : IProperty::Ptr{};
}

IEvent::Ptr resolve_event(const IImportResolver& resolver, string_view path)
{
    // Supported formats:
    //   "object.property.on_changed"  ->  property change event
    //   "object.event_name"           ->  named event (EVT member)
    auto last_dot = path.rfind('.');
    if (last_dot == string_view::npos) {
        return {};
    }

    auto last_segment = path.substr(last_dot + 1);
    auto prefix = path.substr(0, last_dot);

    // "object.property.on_changed" -> resolve as property path, get on_changed
    if (last_segment == string_view("on_changed", 10)) {
        auto prop = resolve_property(resolver, prefix);
        if (prop) {
            return prop->on_changed();
        }
    }

    // "object.event_name" -> resolve object, look up named event
    auto obj = resolver.resolve(prefix);
    if (!obj) {
        return {};
    }
    auto* meta = interface_cast<IMetadata>(obj);
    if (!meta) {
        return {};
    }
    return meta->get_event(last_segment, Resolve::Create);
}

string join_handler_lines(const IImportData& data)
{
    if (data.kind() == IImportData::Kind::String) {
        return string(data.as_string());
    }
    if (data.kind() == IImportData::Kind::Array) {
        string result;
        for (size_t i = 0; i < data.count(); ++i) {
            if (i > 0) {
                result += '\n';
            }
            result += data.at(i).as_string();
        }
        return result;
    }
    return {};
}

JsPlugin* find_js_plugin()
{
    auto& registry = ::velk::instance().plugin_registry();
    auto plugin = registry.find_plugin(PluginId::JsPlugin);
    if (!plugin) {
        return nullptr;
    }
    auto* ijs = interface_cast<IJsPlugin>(plugin);
    return ijs ? static_cast<JsPlugin*>(ijs) : nullptr;
}

// Extract the object id prefix from a dotted path like "obj_id.property_name"
string_view path_object_id(string_view path)
{
    auto dot = path.rfind('.');
    return dot != string_view::npos ? path.substr(0, dot) : path;
}

// Register an object as a JS global, resolving by path prefix.
void ensure_global(JsStoreContext& ctx, const IImportResolver& resolver, string_view id)
{
    auto obj = resolver.resolve(id);
    if (obj) {
        string id_str(id);
        ctx.register_global(id_str.c_str(), obj);
    }
}

} // namespace

string_view JsImportHandler::collection_key() const
{
    return "scripts";
}

void JsImportHandler::process(const IImportData& data, IStore& store,
                              const IImportResolver& resolver) const
{
    if (data.kind() != IImportData::Kind::Array) {
        return;
    }

    auto* plugin = find_js_plugin();
    if (!plugin) {
        return;
    }

    auto& ctx = plugin->get_or_create_context(store);

    // First pass: scan all script entries and register referenced objects as JS globals.
    for (size_t i = 0; i < data.count(); ++i) {
        auto& entry = data.at(i);
        if (entry.kind() != IImportData::Kind::Object) continue;

        auto& target_node = entry.find("target");
        if (!target_node.is_null()) {
            ensure_global(ctx, resolver, path_object_id(target_node.as_string()));
        }
        auto& event_node = entry.find("event");
        if (!event_node.is_null()) {
            ensure_global(ctx, resolver, path_object_id(event_node.as_string()));
        }
        // Scan expression source for object references: any word before a dot
        // that resolves in the store becomes a global.
        auto scan_source = [&](string_view src) {
            // Simple heuristic: find "word.word" patterns and try to resolve the first word.
            size_t pos = 0;
            while (pos < src.size()) {
                // Skip non-identifier chars
                while (pos < src.size() && !((src[pos] >= 'a' && src[pos] <= 'z') ||
                       (src[pos] >= 'A' && src[pos] <= 'Z') || src[pos] == '_')) {
                    ++pos;
                }
                size_t start = pos;
                while (pos < src.size() && ((src[pos] >= 'a' && src[pos] <= 'z') ||
                       (src[pos] >= 'A' && src[pos] <= 'Z') ||
                       (src[pos] >= '0' && src[pos] <= '9') || src[pos] == '_')) {
                    ++pos;
                }
                if (pos > start && pos < src.size() && src[pos] == '.') {
                    auto word = src.substr(start, pos - start);
                    ensure_global(ctx, resolver, word);
                }
            }
        };
        auto& expr_node = entry.find("expr");
        if (!expr_node.is_null()) {
            scan_source(expr_node.as_string());
        }
        auto& handler_node = entry.find("handler");
        if (!handler_node.is_null()) {
            if (handler_node.kind() == IImportData::Kind::String) {
                scan_source(handler_node.as_string());
            } else if (handler_node.kind() == IImportData::Kind::Array) {
                for (size_t j = 0; j < handler_node.count(); ++j) {
                    scan_source(handler_node.at(j).as_string());
                }
            }
        }
    }

    // Second pass: create bindings and event handlers.
    for (size_t i = 0; i < data.count(); ++i) {
        auto& entry = data.at(i);
        if (entry.kind() != IImportData::Kind::Object) {
            continue;
        }

        auto& target_node = entry.find("target");
        auto& expr_node = entry.find("expr");
        auto& event_node = entry.find("event");
        auto& handler_node = entry.find("handler");

        if (!target_node.is_null() && !expr_node.is_null()) {
            auto target_prop = resolve_property(resolver, target_node.as_string());
            if (!target_prop) {
                continue;
            }

            string expr_str(expr_node.as_string());
            JSValue fn = ctx.compile_expression(expr_str.c_str(), "<script-expr>");
            if (JS_IsException(fn)) {
                continue;
            }

            auto js_fn = create_js_function(&ctx, fn);
            if (!js_fn) {
                continue;
            }

            auto binding = create_binding(
                IFunction::ConstPtr(js_fn), InvokeType::Auto);
            if (!binding) {
                VELK_LOG(E, "Failed to create script binding");
                continue;
            }
            if (!binding.add_target(target_prop)) {
                VELK_LOG(E, "Failed to install script binding on target");
            }

        } else if (!event_node.is_null() && !handler_node.is_null()) {
            auto event = resolve_event(resolver, event_node.as_string());
            if (!event) {
                VELK_LOG(E, "Failed to resolve event: %.*s",
                         (int)event_node.as_string().size(), event_node.as_string().data());
                continue;
            }

            string source = join_handler_lines(handler_node);
            if (source.empty()) {
                VELK_LOG(E, "Empty handler source");
                continue;
            }

            JSValue fn = ctx.compile_handler(source.c_str(), "<script-handler>");
            if (JS_IsException(fn)) {
                VELK_LOG(E, "Failed to compile handler");
                continue;
            }

            auto js_fn = create_js_function(&ctx, fn);
            if (!js_fn) {
                VELK_LOG(E, "Failed to create JS function for handler");
                continue;
            }

            event->add_handler(IFunction::ConstPtr(js_fn), InvokeType::Deferred);
        }
    }
}

} // namespace velk

#include "json_import_data.h"
#include "json_importer.h"

#include <velk/api/binding.h>
#include <velk/api/hierarchy.h>
#include <velk/api/store.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_importer_extension.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object_ref.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_type_registry.h>
#include <velk/string.h>

#include <vector>

namespace velk {

namespace {

void add_error(ImportResult& result, const char* msg)
{
    result.errors.push_back(::velk::string(msg));
}

void add_error(ImportResult& result, const std::string& msg)
{
    result.errors.push_back(::velk::string(msg.c_str(), msg.size()));
}

string_view sv(const std::string& s)
{
    return string_view(s.c_str(), s.size());
}

const MemberDesc* find_property_desc(const ClassInfo& info, string_view name)
{
    for (size_t i = 0; i < info.members.size(); i++) {
        if (info.members[i].name == name &&
            (info.members[i].kind == MemberKind::Property ||
             info.members[i].kind == MemberKind::ArrayProperty)) {
            return &info.members[i];
        }
    }
    return nullptr;
}

} // namespace

void JsonImporter::register_class_alias(string_view import_name, Uid class_uid)
{
    aliases_[std::string(import_name.data(), import_name.size())] = class_uid;
}

ImportResult JsonImporter::import_from_json(string_view json) const
{
    ImportResult result;

    JsonValue root;
    std::string parse_error;
    if (!json_parse(json.data(), json.size(), root, parse_error)) {
        add_error(result, parse_error);
        return result;
    }
    if (root.type() != JsonType::Object) {
        add_error(result, "root must be a JSON object");
        return result;
    }

    result.store = ::velk::instance().create<IStore>(ClassId::Store);
    if (!result.store) {
        add_error(result, "failed to create store");
        return result;
    }

    ImportContext ctx;

    // Parse objects
    auto* objects = root.find("objects");
    if (objects && objects->type() == JsonType::Array) {
        for (auto& obj_node : objects->as_array()) {
            if (obj_node.type() != JsonType::Object) {
                continue;
            }
            auto* id_val = obj_node.find("id");
            if (!id_val || id_val->type() != JsonType::String) {
                add_error(result, "object missing 'id' string field");
                continue;
            }
            auto obj = create_object(obj_node, result);
            if (obj) {
                const auto& id_str = id_val->as_string();
                result.store->add(sv(id_str), obj);
                register_imported_object(ctx, obj, obj_node, result);
            }
        }
    }

    auto& store = *result.store;

    // Build hierarchies
    auto* hierarchies = root.find("hierarchies");
    if (hierarchies && hierarchies->type() == JsonType::Object) {
        build_hierarchies(*hierarchies, store, result);
    }

    // Resolve object references
    if (objects && objects->type() == JsonType::Array) {
        resolve_references(store, *objects, ctx, result);
    }

    // Create bindings (top-level array + inline scalar refs handled in resolve_references)
    create_bindings(store, root, ctx, result);

    // Dispatch remaining top-level keys to registered importer extensions
    dispatch_extensions(root, store);

    return result;
}

Uid JsonImporter::resolve_class(const std::string& class_str, ImportResult& result) const
{
    // Try UUID format first
    auto class_sv = sv(class_str);
    if (is_valid_uid_format(class_sv)) {
        Uid uid(class_sv);
        if (::velk::instance().type_registry().get_class_info(uid)) {
            return uid;
        }
    }

    // Try registered aliases
    auto it = aliases_.find(class_str);
    if (it != aliases_.end()) {
        return it->second;
    }

    // Look up by registered class name
    Uid found = ::velk::instance().type_registry().find_class_by_name(class_sv);
    if (found != Uid{}) {
        return found;
    }

    add_error(result, "unknown class: " + class_str);
    return {};
}

IObject::Ptr JsonImporter::create_object(const JsonValue& obj_node, ImportResult& result) const
{
    auto* class_val = obj_node.find("class");
    if (!class_val || class_val->type() != JsonType::String) {
        add_error(result, "object missing 'class' string field");
        return {};
    }

    Uid class_uid = resolve_class(class_val->as_string(), result);
    if (class_uid == Uid{}) {
        return {};
    }

    auto obj = ::velk::instance().create<IObject>(class_uid);
    if (!obj) {
        add_error(result, "failed to create object of class: " + class_val->as_string());
        return {};
    }

    // Get class info for property type dispatch
    auto* info = ::velk::instance().type_registry().get_class_info(class_uid);
    if (!info) {
        return obj;
    }

    // Set properties
    auto* props = obj_node.find("properties");
    if (props && props->type() == JsonType::Object) {
        set_properties(*obj, *props, *info, result);
    }

    return obj;
}

void JsonImporter::set_properties(IObject& obj, const JsonValue& props, const ClassInfo& info,
                                  ImportResult& result) const
{
    auto* meta = interface_cast<IMetadata>(&obj);
    if (!meta) {
        return;
    }

    for (auto& [name, val] : props.as_object()) {
        auto name_sv = sv(name);
        const MemberDesc* desc = find_property_desc(info, name_sv);
        if (!desc) {
            add_error(result, "unknown property '" + name + "' on class '" +
                              std::string(info.name.data(), info.name.size()) + "'");
            continue;
        }

        auto prop = meta->get_property(name_sv);
        if (!prop) {
            continue;
        }

        auto* pi = interface_cast<IPropertyInternal>(prop);
        if (!pi) {
            continue;
        }

        auto* pk = desc->propertyKind();
        if (!pk) {
            continue;
        }

        // Handle object form: { "value": ..., "flags": ... }
        const JsonValue* value_node = &val;
        if (val.type() == JsonType::Object) {
            if (val.find("ref")) {
                continue;
            }
            auto* inner = val.find("value");
            if (inner) {
                value_node = inner;
            }
        }

        set_property_value(*pi, *pk, *value_node);
    }
}

void JsonImporter::set_property_value(IPropertyInternal& pi, const PropertyKind& pk,
                                      const JsonValue& val) const
{
    Uid typeUid = pk.typeUid;

    if (typeUid == type_uid<float>()) {
        if (val.type() != JsonType::Number) return;
        float v = static_cast<float>(val.as_number());
        pi.set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<double>()) {
        if (val.type() != JsonType::Number) return;
        double v = val.as_number();
        pi.set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<int32_t>()) {
        if (val.type() != JsonType::Number) return;
        int32_t v = static_cast<int32_t>(val.as_number());
        pi.set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<uint32_t>()) {
        if (val.type() != JsonType::Number) return;
        uint32_t v = static_cast<uint32_t>(val.as_number());
        pi.set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<int64_t>()) {
        if (val.type() != JsonType::Number) return;
        int64_t v = static_cast<int64_t>(val.as_number());
        pi.set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<uint64_t>()) {
        if (val.type() != JsonType::Number) return;
        uint64_t v = static_cast<uint64_t>(val.as_number());
        pi.set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<bool>()) {
        if (val.type() != JsonType::Bool) return;
        bool v = val.as_bool();
        pi.set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<int>()) {
        if (val.type() != JsonType::Number) return;
        int v = static_cast<int>(val.as_number());
        pi.set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<::velk::string>()) {
        if (val.type() != JsonType::String) return;
        ::velk::string v(val.as_string().c_str(), val.as_string().size());
        pi.set_data(&v, sizeof(v), typeUid);
    }
}

void JsonImporter::register_imported_object(ImportContext& ctx, IObject::Ptr obj,
                                            const JsonValue& obj_node, ImportResult& result) const
{
    auto* id_val = obj_node.find("id");
    const auto& id_str = id_val->as_string();

    ctx.name_to_object[id_str] = obj;
    auto* name_val = obj_node.find("name");
    if (name_val && name_val->type() == JsonType::String) {
        ctx.name_to_object[name_val->as_string()] = obj;
        ctx.object_to_name[obj.get()] = name_val->as_string();
    } else {
        ctx.object_to_name[obj.get()] = id_str;
    }

    auto* class_val = obj_node.find("class");
    if (class_val && class_val->type() == JsonType::String) {
        Uid uid = resolve_class(class_val->as_string(), result);
        if (uid != Uid{}) {
            ctx.object_to_class_info[obj.get()] =
                ::velk::instance().type_registry().get_class_info(uid);
        }
    }
}

void JsonImporter::build_hierarchies(const JsonValue& hierarchies, IStore& store,
                                     ImportResult& result) const
{
    for (auto& [name, hierarchy_val] : hierarchies.as_object()) {
        if (hierarchy_val.type() != JsonType::Array) {
            add_error(result, "hierarchy '" + name + "' is not an array");
            continue;
        }
        build_hierarchy(name, hierarchy_val, store, result);
    }
}

void JsonImporter::build_hierarchy(const std::string& name, const JsonValue& edges_json,
                                   IStore& store, ImportResult& result) const
{
    auto hierarchy = ::velk::create_hierarchy();
    if (!hierarchy) {
        add_error(result, "failed to create hierarchy '" + name + "'");
        return;
    }

    // Parse edges: array of { "parent": "id", "child": "id" }
    // First pass: collect all edges and determine root
    struct Edge
    {
        std::string parent;
        std::string child;
    };
    std::vector<Edge> edges;
    std::unordered_map<std::string, bool> has_parent;

    for (auto& edge : edges_json.as_array()) {
        if (edge.type() != JsonType::Object) {
            continue;
        }
        auto* parent_val = edge.find("parent");
        auto* child_val = edge.find("child");
        if (!parent_val || parent_val->type() != JsonType::String || !child_val ||
            child_val->type() != JsonType::String) {
            continue;
        }
        edges.push_back({parent_val->as_string(), child_val->as_string()});
        has_parent[child_val->as_string()] = true;
        if (has_parent.find(parent_val->as_string()) == has_parent.end()) {
            has_parent[parent_val->as_string()] = false;
        }
    }

    // Find root: a node that appears as parent but never as child
    std::string root_id;
    for (auto& [id, is_child] : has_parent) {
        if (!is_child) {
            root_id = id;
            break;
        }
    }

    if (root_id.empty() && !edges.empty()) {
        root_id = edges[0].parent;
    }

    if (!root_id.empty()) {
        auto root_obj = store.find(sv(root_id));
        if (root_obj) {
            hierarchy.set_root(root_obj);

            for (auto& edge : edges) {
                auto parent_obj = store.find(sv(edge.parent));
                auto child_obj = store.find(sv(edge.child));
                if (parent_obj && child_obj) {
                    hierarchy.add(parent_obj, child_obj);
                } else {
                    if (!parent_obj) {
                        add_error(result, "hierarchy '" + name +
                                          "': parent object not found: " + edge.parent);
                    }
                    if (!child_obj) {
                        add_error(result, "hierarchy '" + name +
                                          "': child object not found: " + edge.child);
                    }
                }
            }
        } else {
            add_error(result, "hierarchy '" + name + "': root object not found: " + root_id);
        }
    }

    // Add hierarchy to store with key "hierarchy:<name>"
    auto store_key = ::velk::string("hierarchy:") + ::velk::string_view(name.c_str(), name.size());
    auto hierarchy_obj = interface_pointer_cast<IObject>(hierarchy.as_ptr<IHierarchy>());
    if (hierarchy_obj) {
        store.add(store_key, hierarchy_obj);
    }
}

IObject::Ptr JsonImporter::resolve_object(IStore& store, const ImportContext& ctx,
                                          const std::string& path) const
{
    if (path.empty()) {
        return {};
    }

    if (path[0] == '/') {
        return resolve_object_by_path(store, ctx, path);
    }

    // Direct name: try name map first (includes both ids and names), then store lookup
    auto it = ctx.name_to_object.find(path);
    if (it != ctx.name_to_object.end()) {
        return it->second;
    }
    return store.find(sv(path));
}

IObject::Ptr JsonImporter::resolve_object_by_path(IStore& store, const ImportContext& ctx,
                                                   const std::string& path) const
{
    // Hierarchy path: /segment1/segment2/...
    std::vector<std::string> segments;
    size_t start = 1;
    while (start < path.size()) {
        auto end = path.find('/', start);
        if (end == std::string::npos) {
            end = path.size();
        }
        if (end > start) {
            segments.push_back(path.substr(start, end - start));
        }
        start = end + 1;
    }
    if (segments.empty()) {
        return {};
    }

    // Find hierarchy to walk
    IHierarchy* hierarchy = nullptr;
    size_t start_segment = 0;

    // Try first segment as hierarchy name
    auto h_key = "hierarchy:" + segments[0];
    auto h_obj = store.find(sv(h_key));
    if (h_obj) {
        hierarchy = interface_cast<IHierarchy>(h_obj);
        if (hierarchy) {
            start_segment = 1;
        }
    }

    // If not found by name, find any hierarchy in the store
    if (!hierarchy) {
        for (size_t i = 0; i < store.object_count(); i++) {
            auto obj = store.object_at(i);
            auto* h = interface_cast<IHierarchy>(obj);
            if (h) {
                hierarchy = h;
                break;
            }
        }
    }

    if (!hierarchy || segments.size() <= start_segment) {
        return {};
    }

    // Walk from root, matching names at each level
    auto current = hierarchy->root();
    if (!current) {
        return {};
    }

    // Match root against first segment
    auto root_it = ctx.object_to_name.find(current.get());
    if (root_it == ctx.object_to_name.end() || root_it->second != segments[start_segment]) {
        return {};
    }
    start_segment++;

    // Walk remaining segments through children
    for (size_t i = start_segment; i < segments.size(); i++) {
        IObject::Ptr found;
        auto children = hierarchy->children_of(current);
        for (auto& child : children) {
            auto it = ctx.object_to_name.find(child.get());
            if (it != ctx.object_to_name.end() && it->second == segments[i]) {
                found = child;
                break;
            }
        }
        if (!found) {
            return {};
        }
        current = found;
    }

    return current;
}

IProperty::Ptr JsonImporter::resolve_property(IStore& store, const ImportContext& ctx,
                                              const std::string& path) const
{
    auto dot = path.rfind('.');
    if (dot == std::string::npos) {
        return {};
    }

    auto obj_path = path.substr(0, dot);
    auto prop_name = path.substr(dot + 1);

    auto obj = resolve_object(store, ctx, obj_path);
    if (!obj) {
        return {};
    }

    auto* meta = interface_cast<IMetadata>(obj);
    if (!meta) {
        return {};
    }

    return meta->get_property(sv(prop_name));
}

void JsonImporter::resolve_references(IStore& store, const JsonValue& objects,
                                      const ImportContext& ctx, ImportResult& result) const
{
    for (auto& obj_node : objects.as_array()) {
        if (obj_node.type() != JsonType::Object) {
            continue;
        }

        auto* id_val = obj_node.find("id");
        if (!id_val || id_val->type() != JsonType::String) {
            continue;
        }

        auto obj = store.find(sv(id_val->as_string()));
        if (!obj) {
            continue;
        }

        auto* props = obj_node.find("properties");
        if (!props || props->type() != JsonType::Object) {
            continue;
        }

        // Use cached class info instead of re-resolving
        auto info_it = ctx.object_to_class_info.find(obj.get());
        if (info_it == ctx.object_to_class_info.end() || !info_it->second) {
            continue;
        }
        const ClassInfo* info = info_it->second;

        auto* meta = interface_cast<IMetadata>(obj);
        if (!meta) {
            continue;
        }

        for (auto& [name, val] : props->as_object()) {
            if (val.type() != JsonType::Object) {
                continue;
            }

            auto* ref_val = val.find("ref");
            if (!ref_val || ref_val->type() != JsonType::String) {
                continue;
            }

            const std::string& ref_path = ref_val->as_string();
            auto name_sv = sv(name);

            const MemberDesc* desc = find_property_desc(*info, name_sv);
            if (!desc) {
                continue;
            }

            auto* pk = desc->propertyKind();
            if (!pk) {
                continue;
            }

            if (pk->typeUid == ClassId::ObjectRef) {
                resolve_object_ref(*meta, name, val, store, ctx, result);
            } else {
                resolve_inline_binding(*meta, name, ref_path, store, ctx, result);
            }
        }
    }
}

void JsonImporter::resolve_object_ref(IMetadata& meta, const std::string& name,
                                      const JsonValue& ref_node, IStore& store,
                                      const ImportContext& ctx, ImportResult& result) const
{
    auto* ref_val = ref_node.find("ref");
    const std::string& ref_path = ref_val->as_string();

    auto resolved = resolve_object(store, ctx, ref_path);
    if (!resolved) {
        add_error(result, "unresolved object reference '" + ref_path + "' on property '" + name + "'");
        return;
    }

    auto prop = meta.get_property(sv(name));
    if (!prop) {
        return;
    }

    auto obj_ref = ::velk::instance().create_object_ref();
    if (!obj_ref) {
        return;
    }

    auto* type_val = ref_node.find("type");
    if (type_val && type_val->type() == JsonType::String && type_val->as_string() == "weak") {
        obj_ref->set_owning(false);
    }
    obj_ref->set_object(resolved);

    auto* pi = interface_cast<IPropertyInternal>(prop);
    if (pi) {
        pi->set_any(interface_pointer_cast<IAny>(obj_ref));
    }
}

void JsonImporter::resolve_inline_binding(IMetadata& meta, const std::string& name,
                                          const std::string& ref_path, IStore& store,
                                          const ImportContext& ctx, ImportResult& result) const
{
    auto source_prop = resolve_property(store, ctx, ref_path);
    if (!source_prop) {
        add_error(result, "unresolved property reference '" + ref_path + "' on '" + name + "'");
        return;
    }

    auto target_prop = meta.get_property(sv(name));
    if (!target_prop) {
        return;
    }

    auto binding = ::velk::create_binding(source_prop);
    binding.add_target(target_prop);
}

void JsonImporter::create_bindings(IStore& store, const JsonValue& root, const ImportContext& ctx,
                                   ImportResult& result) const
{
    auto* bindings_val = root.find("bindings");
    if (!bindings_val || bindings_val->type() != JsonType::Array) {
        return;
    }

    for (auto& binding_node : bindings_val->as_array()) {
        if (binding_node.type() != JsonType::Object) {
            continue;
        }
        parse_binding(binding_node, store, ctx, result);
    }
}

void JsonImporter::parse_binding(const JsonValue& binding_node, IStore& store,
                                 const ImportContext& ctx, ImportResult& result) const
{
    auto* source_val = binding_node.find("source");
    if (!source_val || source_val->type() != JsonType::String) {
        add_error(result, "binding missing 'source' string");
        return;
    }

    auto* targets_val = binding_node.find("targets");
    if (!targets_val || targets_val->type() != JsonType::Array) {
        add_error(result, "binding missing 'targets' array");
        return;
    }

    auto source_prop = resolve_property(store, ctx, source_val->as_string());
    if (!source_prop) {
        add_error(result, "binding: unresolved source '" + source_val->as_string() + "'");
        return;
    }

    // Determine binding mode
    BindingMode mode = BindingMode::OneWay;
    auto* mode_val = binding_node.find("mode");
    if (mode_val && mode_val->type() == JsonType::String && mode_val->as_string() == "twoway") {
        mode = BindingMode::TwoWay;
    }

    // Determine invoke type
    InvokeType invoke = Auto;
    auto* invoke_val = binding_node.find("invoke");
    if (invoke_val && invoke_val->type() == JsonType::String) {
        if (invoke_val->as_string() == "immediate") {
            invoke = Immediate;
        } else if (invoke_val->as_string() == "deferred") {
            invoke = Deferred;
        }
    }

    // Create the binding
    auto binding = ::velk::create_binding(source_prop, invoke, mode);
    if (!binding) {
        add_error(result, "failed to create binding");
        return;
    }

    // Add targets
    for (auto& target_node : targets_val->as_array()) {
        if (target_node.type() != JsonType::String) {
            continue;
        }

        auto target_prop = resolve_property(store, ctx, target_node.as_string());
        if (!target_prop) {
            add_error(result, "binding: unresolved target '" + target_node.as_string() + "'");
            continue;
        }

        binding.add_target(target_prop);
    }
}

void JsonImporter::dispatch_extensions(const JsonValue& root, IStore& store) const
{
    struct ExtensionEntry
    {
        IInterface::Ptr instance;
        IImporterExtension* ext;
    };

    // Discover all classes implementing IImporterExtension
    std::vector<ExtensionEntry> extensions;
    auto& registry = ::velk::instance().type_registry();

    struct VisitorCtx
    {
        ITypeRegistry* registry;
        std::vector<ExtensionEntry>* extensions;
    };
    VisitorCtx vctx{&registry, &extensions};

    registry.for_each_class(&vctx, [](void* ctx, const ClassInfo& info) -> bool {
        auto* vc = static_cast<VisitorCtx*>(ctx);
        // Check if this class implements IImporterExtension
        for (size_t i = 0; i < info.interfaces.size(); i++) {
            if (info.interfaces[i].uid == IImporterExtension::UID) {
                auto instance = vc->registry->create(info.uid);
                if (instance) {
                    auto* ext = interface_cast<IImporterExtension>(instance);
                    if (ext) {
                        vc->extensions->push_back({instance, ext});
                    }
                }
                break;
            }
        }
        return true;
    });

    // Dispatch each extension's collection key
    for (auto& entry : extensions) {
        auto key = entry.ext->collection_key();
        std::string key_str(key.data(), key.size());
        auto* json_node = root.find(key_str);
        if (json_node) {
            JsonImportData wrapped(*json_node);
            entry.ext->process(wrapped, store);
        }
    }
}

} // namespace velk

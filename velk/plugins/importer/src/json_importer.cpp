#include "json_importer.h"

#include <velk/api/hierarchy.h>
#include <velk/api/store.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_type_registry.h>
#include <velk/string.h>

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
                result.store->add(
                    string_view(id_val->as_string().c_str(), id_val->as_string().size()), obj);
            }
        }
    }

    // Build hierarchies
    auto* hierarchies = root.find("hierarchies");
    if (hierarchies && hierarchies->type() == JsonType::Object) {
        build_hierarchies(*hierarchies, result.store.get(), result);
    }

    return result;
}

Uid JsonImporter::resolve_class(const std::string& class_str, ImportResult& result) const
{
    // Try UUID format first
    auto sv = string_view(class_str.c_str(), class_str.size());
    if (is_valid_uid_format(sv)) {
        Uid uid(sv);
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
    Uid found = ::velk::instance().type_registry().find_class_by_name(sv);
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
        set_properties(obj.get(), *props, *info, result);
    }

    return obj;
}

void JsonImporter::set_properties(IObject* obj, const JsonValue& props, const ClassInfo& info,
                                  ImportResult& result) const
{
    auto* meta = interface_cast<IMetadata>(obj);
    if (!meta) {
        return;
    }

    for (auto& [name, val] : props.as_object()) {
        // Find the member descriptor for this property name
        auto name_sv = string_view(name.c_str(), name.size());
        const MemberDesc* desc = nullptr;
        for (size_t i = 0; i < info.members.size(); i++) {
            if (info.members[i].name == name_sv &&
                (info.members[i].kind == MemberKind::Property ||
                 info.members[i].kind == MemberKind::ArrayProperty)) {
                desc = &info.members[i];
                break;
            }
        }
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
            // Check for reference (skip for phase 1)
            if (val.find("ref")) {
                continue;
            }
            auto* inner = val.find("value");
            if (inner) {
                value_node = inner;
            }
        }

        set_property_value(pi, *pk, *value_node);
    }
}

void JsonImporter::set_property_value(IPropertyInternal* pi, const PropertyKind& pk,
                                      const JsonValue& val) const
{
    Uid typeUid = pk.typeUid;

    if (typeUid == type_uid<float>()) {
        if (val.type() != JsonType::Number) return;
        float v = static_cast<float>(val.as_number());
        pi->set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<double>()) {
        if (val.type() != JsonType::Number) return;
        double v = val.as_number();
        pi->set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<int32_t>()) {
        if (val.type() != JsonType::Number) return;
        int32_t v = static_cast<int32_t>(val.as_number());
        pi->set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<uint32_t>()) {
        if (val.type() != JsonType::Number) return;
        uint32_t v = static_cast<uint32_t>(val.as_number());
        pi->set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<int64_t>()) {
        if (val.type() != JsonType::Number) return;
        int64_t v = static_cast<int64_t>(val.as_number());
        pi->set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<uint64_t>()) {
        if (val.type() != JsonType::Number) return;
        uint64_t v = static_cast<uint64_t>(val.as_number());
        pi->set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<bool>()) {
        if (val.type() != JsonType::Bool) return;
        bool v = val.as_bool();
        pi->set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<int>()) {
        if (val.type() != JsonType::Number) return;
        int v = static_cast<int>(val.as_number());
        pi->set_data(&v, sizeof(v), typeUid);
    } else if (typeUid == type_uid<::velk::string>()) {
        if (val.type() != JsonType::String) return;
        ::velk::string v(val.as_string().c_str(), val.as_string().size());
        pi->set_data(&v, sizeof(v), typeUid);
    }
}

void JsonImporter::build_hierarchies(const JsonValue& hierarchies, IStore* store,
                                     ImportResult& result) const
{
    for (auto& [name, hierarchy_val] : hierarchies.as_object()) {
        if (hierarchy_val.type() != JsonType::Array) {
            add_error(result, "hierarchy '" + name + "' is not an array");
            continue;
        }

        auto hierarchy = ::velk::create_hierarchy();
        if (!hierarchy) {
            add_error(result, "failed to create hierarchy '" + name + "'");
            continue;
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

        for (auto& edge : hierarchy_val.as_array()) {
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
            auto root_sv = string_view(root_id.c_str(), root_id.size());
            auto root_obj = store->find(root_sv);
            if (root_obj) {
                hierarchy.set_root(root_obj);

                for (auto& edge : edges) {
                    auto parent_sv = string_view(edge.parent.c_str(), edge.parent.size());
                    auto child_sv = string_view(edge.child.c_str(), edge.child.size());
                    auto parent_obj = store->find(parent_sv);
                    auto child_obj = store->find(child_sv);
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
            store->add(store_key, hierarchy_obj);
        }
    }
}

} // namespace velk

#ifndef VELK_IMPORTER_JSON_IMPORTER_H
#define VELK_IMPORTER_JSON_IMPORTER_H

#include "json_parser.h"

#include <velk/interface/intf_metadata.h>
#include <velk/interface/member_desc.h>
#include <velk/interface/types.h>
#include <velk/plugins/importer/interface/intf_importer_plugin.h>

#include <string>
#include <unordered_map>

namespace velk {

class JsonImporter
{
public:
    void register_class_alias(string_view import_name, Uid class_uid);
    ImportResult import_from_json(string_view json) const;

private:
    struct ImportContext
    {
        std::unordered_map<std::string, IObject::Ptr> name_to_object;
        std::unordered_map<IObject*, std::string> object_to_name;
        std::unordered_map<IObject*, const ClassInfo*> object_to_class_info;
    };

    Uid resolve_class(const std::string& class_str, ImportResult& result) const;
    IObject::Ptr create_object(const JsonValue& obj_node, ImportResult& result) const;
    void set_properties(IObject& obj, const JsonValue& props, const ClassInfo& info,
                        ImportResult& result) const;
    void set_property_value(IPropertyInternal& pi, const PropertyKind& pk, const JsonValue& val) const;
    void register_imported_object(ImportContext& ctx, IObject::Ptr obj, const JsonValue& obj_node,
                                  ImportResult& result) const;
    void build_hierarchies(const JsonValue& hierarchies, IStore& store, ImportResult& result) const;
    void build_hierarchy(const std::string& name, const JsonValue& edges_json, IStore& store,
                         ImportResult& result) const;
    void resolve_references(IStore& store, const JsonValue& objects, const ImportContext& ctx,
                            ImportResult& result) const;
    void resolve_object_ref(IMetadata& meta, const std::string& name, const JsonValue& ref_node,
                            IStore& store, const ImportContext& ctx, ImportResult& result) const;
    void resolve_inline_binding(IMetadata& meta, const std::string& name, const std::string& ref_path,
                                IStore& store, const ImportContext& ctx, ImportResult& result) const;
    void create_bindings(IStore& store, const JsonValue& root, const ImportContext& ctx,
                         ImportResult& result) const;
    void parse_binding(const JsonValue& binding_node, IStore& store, const ImportContext& ctx,
                       ImportResult& result) const;
    IObject::Ptr resolve_object(IStore& store, const ImportContext& ctx,
                                const std::string& path) const;
    IObject::Ptr resolve_object_by_path(IStore& store, const ImportContext& ctx,
                                        const std::string& path) const;
    IProperty::Ptr resolve_property(IStore& store, const ImportContext& ctx,
                                    const std::string& path) const;
    void dispatch_extensions(const JsonValue& root, IStore& store) const;

    std::unordered_map<std::string, Uid> aliases_;
};

} // namespace velk

#endif // VELK_IMPORTER_JSON_IMPORTER_H

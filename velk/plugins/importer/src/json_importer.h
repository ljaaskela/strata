#ifndef VELK_IMPORTER_JSON_IMPORTER_H
#define VELK_IMPORTER_JSON_IMPORTER_H

#include "json_parser.h"

#include <velk/ext/core_object.h>
#include <velk/interface/intf_importer_extension.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/member_desc.h>
#include <velk/interface/types.h>
#include <velk/plugins/importer/interface/intf_importer_plugin.h>
#include <velk/plugins/importer/plugin.h>

#include <velk/string.h>

#include <unordered_map>

namespace velk {

class JsonImporter : public ext::ObjectCore<JsonImporter, IStoreImporter>
{
public:
    VELK_CLASS_UID(ClassId::JsonImporter, "JsonImporter");

    ImportResult import_from(string_view json) const override;

private:
    friend class ImportResolver;

    struct ImportContext
    {
        std::unordered_map<std::string, IObject::Ptr> name_to_object;
        std::unordered_map<IObject*, string> object_to_name;
        std::unordered_map<IObject*, const ClassInfo*> object_to_class_info;
    };

    Uid resolve_class(string_view class_str, ImportResult& result) const;
    IObject::Ptr create_object(const JsonValue& obj_node, ImportResult& result) const;
    void set_properties(IObject& obj, const JsonValue& props, const ClassInfo& info,
                        ImportResult& result) const;
    void set_property_value(IPropertyInternal& pi, const PropertyKind& pk, const JsonValue& val) const;
    void register_imported_object(ImportContext& ctx, IObject::Ptr obj, const JsonValue& obj_node,
                                  ImportResult& result) const;
    void build_hierarchies(const JsonValue& hierarchies, IStore& store, ImportResult& result) const;
    void build_hierarchy(string_view name, const JsonValue& tree_json, IStore& store,
                         ImportResult& result) const;
    void resolve_references(IStore& store, const JsonValue& objects, const ImportContext& ctx,
                            ImportResult& result) const;
    void resolve_object_ref(IMetadata& meta, string_view name, const JsonValue& ref_node,
                            IStore& store, const ImportContext& ctx, ImportResult& result) const;
    void resolve_inline_binding(IMetadata& meta, string_view name, string_view ref_path,
                                IStore& store, const ImportContext& ctx, ImportResult& result) const;
    void create_bindings(IStore& store, const JsonValue& root, const ImportContext& ctx,
                         ImportResult& result) const;
    void parse_binding(const JsonValue& binding_node, IStore& store, const ImportContext& ctx,
                       ImportResult& result) const;
    IObject::Ptr resolve_object(IStore& store, const ImportContext& ctx,
                                string_view path) const;
    IObject::Ptr resolve_object_by_path(IStore& store, const ImportContext& ctx,
                                        string_view path) const;
    IProperty::Ptr resolve_property(IStore& store, const ImportContext& ctx,
                                    string_view path) const;
    void process_attachments(const JsonValue& root, IStore& store, const ImportContext& ctx,
                             ImportResult& result) const;
    void process_resource_protocols(const JsonValue& root, ImportResult& result) const;
    void process_resources(const JsonValue& root, IStore& store, ImportContext& ctx,
                           ImportResult& result) const;
    void dispatch_extensions(const JsonValue& root, IStore& store, const ImportContext& ctx) const;
    void discover_type_extensions() const;

    mutable std::unordered_map<Uid, IImporterTypeExtension::Ptr> type_extensions_;
};

} // namespace velk

#endif // VELK_IMPORTER_JSON_IMPORTER_H

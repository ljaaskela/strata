#ifndef VELK_JS_IMPORT_HANDLER_H
#define VELK_JS_IMPORT_HANDLER_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_importer_extension.h>
#include <velk/plugins/script/js/plugin.h>

namespace velk {

class JsPlugin;

/**
 * @brief Importer extension that handles the "scripts" top-level collection.
 *
 * Each entry creates either an expression binding or an event handler.
 *
 * JSON format:
 *   "scripts": [
 *       {
 *           "target": "object_id.property_name",
 *           "expr": "parent.width * 0.5"
 *       },
 *       {
 *           "event": "object_id.event_name",
 *           "handler": "label.text = 'Clicked!'"
 *       }
 *   ]
 */
class JsImportHandler : public ext::ObjectCore<JsImportHandler, IImporterExtension>
{
public:
    VELK_CLASS_UID(ClassId::JsImportHandler, "JsImportHandler");

    string_view collection_key() const override;
    void process(const IImportData& data, IStore& store,
                 const IImportResolver& resolver) const override;
};

} // namespace velk

#endif // VELK_JS_IMPORT_HANDLER_H
